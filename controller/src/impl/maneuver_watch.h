// FLvibed - the maneuver watchdog's decision rule (0.2.1 hotfix, 2 Nov 2026). Header only; unit test
// tests\maneuver_watch_test.cpp; the live part is controller.cpp WatchManeuver (setting `maneuver_watchdog`).
//
// The owner's player report (M01, real Xbox pad): in formation with the Transport USV Brandt, at the mission's
// LockManeuvers the ship banked into a constant hard turn, braked 80 -> 1 m/s and kept turning in place while the
// convoy flew away; leaving was refused (the lock). The owner: "ship controls and autopilot capabilities must be robust
// to capricious ship/solar targeting changes". So while the player's ship runs a game maneuver with a leader / goal
// (formation, goto, dock) and the Advanced Autopilot is not flying: a ship slower than kMinSpeed for kStallSecs while
// the goal is farther than kFarDist and not getting closer is stalled -> one snapshot line, then the same maneuver is
// issued again on the same leader / goal, at most once per kRetrySecs and kMaxTries times per stall. A player who is
// steering (stick / mouse in the last second) is never fought: the stall clock stops while they steer.
#pragma once

namespace mwatch
{
constexpr float kMinSpeed = 5.0f;   // m/s: slower is "stopped"
constexpr float kStallSecs = 3.0f;  // stopped this long while far = stalled
constexpr float kFarDist = 150.0f;  // m: closer to the leader / goal is "there"
constexpr float kShrinkBy = 20.0f;  // m: the distance fell by this much since the clock started = still closing in
constexpr float kRetrySecs = 10.0f; // between two re-issues
constexpr int kMaxTries = 3;        // re-issues per stall

struct Sample
{
    float t = 0;            // seconds (any monotonic clock)
    bool maneuver = false;  // a formation / goto / dock with a known leader / goal runs
    unsigned key = 0;       // which maneuver (op and goal): a new one starts the watch afresh
    bool autopilot = false; // the Advanced Autopilot flies (it has its own stall handling)
    bool steering = false;  // the player steered (stick / mouse) in the last second
    float speed = 0;        // m/s
    float dist = 0;         // m to the leader / goal
};

struct Decision
{
    bool snapshot = false; // log the stall's snapshot (once per stall)
    bool retry = false;    // issue the maneuver again now
    int attempt = 0;       // the retry's number (1..kMaxTries)
    bool gaveUp = false;   // the tries are spent (once per stall)
};

class Watch
{
  public:
    Decision Step(const Sample& s)
    {
        Decision d;
        if (!s.maneuver || s.autopilot || s.key != key_)
        {
            Reset();
            key_ = s.maneuver && !s.autopilot ? s.key : 0;
            if (!s.maneuver || s.autopilot)
                return d;
        }
        if (s.speed >= kMinSpeed || s.dist <= kFarDist)
        {
            Reset(); // moving or there: whatever stalled is over
            key_ = s.key;
            return d;
        }
        if (s.steering)
        {
            since_ = -1; // the player flies: the clock stops (tries already spent stay spent)
            return d;
        }
        if (since_ < 0 || s.dist < ref_ - kShrinkBy)
        {
            since_ = s.t, ref_ = s.dist; // (re)start the clock: first slow frame, or still closing in
            return d;
        }
        if (s.t - since_ < kStallSecs)
            return d;
        if (!stalled_)
        {
            stalled_ = true;
            d.snapshot = true;
        }
        if (tries_ < kMaxTries && (tries_ == 0 || s.t - lastTry_ >= kRetrySecs))
        {
            ++tries_;
            lastTry_ = s.t;
            d.retry = true;
            d.attempt = tries_;
        }
        else if (tries_ >= kMaxTries && !gaveUp_ && s.t - lastTry_ >= kRetrySecs)
        {
            gaveUp_ = true;
            d.gaveUp = true;
        }
        return d;
    }
    bool Stalled() const { return stalled_; }
    int Tries() const { return tries_; }

  private:
    void Reset()
    {
        since_ = -1, ref_ = 0, lastTry_ = 0;
        tries_ = 0;
        stalled_ = gaveUp_ = false;
    }
    unsigned key_ = 0;
    float since_ = -1, ref_ = 0, lastTry_ = 0;
    int tries_ = 0;
    bool stalled_ = false, gaveUp_ = false;
};
} // namespace mwatch
