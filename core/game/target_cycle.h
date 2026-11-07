// FLvibed - the targeting press's enemy rule. Header only; unit test tests\target_cycle_test.cpp;
// the live part is controller.cpp SmartTarget.
//
// The owner, 7 Nov 2026, over 4 Nov's rule: reticle first - a cold press takes what the reticle is on (the caller's ring
// pick; Cold / Took below), and only with nothing there the enemies. The owner, 4 Nov 2026: "right stick should default
// to target nearest enemy for a first press, and only consecutive presses should be 'next target'". A cold press takes the nearest enemy; a press within kChainSecs of the last one, with the target it
// left still selected, takes the nearest enemy this run of presses has not visited, so a run walks every enemy once even
// while they move, then starts over. A cold press on the nearest enemy goes on to the next one: a press always does
// something when there is anything else to take. The double tap (clear) is the preset's, not this rule's.
#pragma once

namespace tcycle
{
constexpr float kChainSecs = 3.0f;      // a press this soon after the last one continues the run
constexpr float kReach = 10000.0f;      // m: an enemy past this is not "nearby" (the threat rows' reach)
constexpr int kMaxSeen = 32;

struct Enemy
{
    unsigned id = 0;
    float dist = 0; // m to the player
};

class Chain
{
  public:
    // One press at time `t` (seconds, any monotonic clock) with `cur` targeted: the enemy to target, 0 = none to take
    // (no enemies, or the only one is already the target).
    unsigned Press(float t, const Enemy* e, int n, unsigned cur)
    {
        const bool chained = run_ && cur == last_ && t - at_ <= kChainSecs;
        if (!chained)
            seen_n_ = 0;
        at_ = t;
        run_ = true;
        if (cur)
            Mark(cur);
        unsigned pick = Nearest(e, n);
        if (!pick && seen_n_ > 0) // every enemy visited: the run starts over, the current target still skipped
        {
            seen_n_ = 0;
            if (cur)
                Mark(cur);
            pick = Nearest(e, n);
        }
        if (pick)
        {
            Mark(pick);
            last_ = pick;
        }
        else
            last_ = cur;
        return pick;
    }

    // The game refused the pick: the run goes on from `cur` with the pick still visited, so the next press takes another.
    void Refused(unsigned cur) { last_ = cur; }

    // Reticle first (the owner, 7 Nov 2026: "unintuitive that I couldn't one-click select the thing I was looking at with
    // enemies nearby"): a cold press takes what the reticle is on before any enemy, so this says whether a press at `t`
    // with `cur` targeted is cold. Took() records that pick as the run's first, so the presses after it walk the enemies.
    bool Cold(float t, unsigned cur) const { return !(run_ && cur == last_ && t - at_ <= kChainSecs); }
    void Took(float t, unsigned id)
    {
        seen_n_ = 0;
        Mark(id);
        last_ = id;
        at_ = t;
        run_ = true;
    }

  private:
    bool Seen(unsigned id) const
    {
        for (int i = 0; i < seen_n_; ++i)
            if (seen_[i] == id)
                return true;
        return false;
    }
    void Mark(unsigned id)
    {
        if (Seen(id))
            return;
        if (seen_n_ >= kMaxSeen) // (more enemies than the list holds: the run starts over)
            seen_n_ = 0;
        seen_[seen_n_++] = id;
    }
    unsigned Nearest(const Enemy* e, int n) const
    {
        unsigned best = 0;
        float bestD = 0;
        for (int i = 0; i < n; ++i)
            if (e[i].id && !Seen(e[i].id) && (!best || e[i].dist < bestD))
                best = e[i].id, bestD = e[i].dist;
        return best;
    }
    unsigned seen_[kMaxSeen] = {};
    int seen_n_ = 0;
    unsigned last_ = 0;
    float at_ = 0;
    bool run_ = false; // a press happened: last_ / at_ mean something
};
} // namespace tcycle
