// Advanced Autopilot: small shared helpers for idioms that were copied between its parts (checkup 24 Oct 2026, audit
// step 4). Header-only, no state of its own: each helper takes its site's state and constants as arguments - the
// numbers stay at the call sites with their provenance (they were measured per site and are never unified here).
#pragma once
#include "autopilot.h"

#include "../events.h"

namespace autopilot
{

// An event / log throttle: Due() is true at most once per `every` seconds and stamps the time when it is. Reset-safe:
// g.time restarts at 0 with every mode start, so a stamp from an earlier run (now < at) counts as due at once.
struct Throttle
{
    float at = -10;
    bool Due(float now, float every)
    {
        if (now - at > every || now < at)
        {
            at = now;
            return true;
        }
        return false;
    }
};

// "Best so far and when": a no-progress watch. Gained() takes a value below the best by more than `slack` as the new
// best (stamped `now`); Since() is the time since the last gain.
struct Progress
{
    float best = 1e30f, bestAt = -10;
    void Reset(float now)
    {
        best = 1e30f;
        bestAt = now;
    }
    bool Gained(float v, float slack, float now)
    {
        if (v < best - slack)
        {
            best = v;
            bestAt = now;
            return true;
        }
        return false;
    }
    float Since(float now) const { return now - bestAt; }
    // no gain of `slack` for more than `patience` s (a gain is taken first)
    bool Stalled(float v, float slack, float now, float patience)
    {
        return !Gained(v, slack, now) && Since(now) > patience;
    }
};

// Fly towards `pos` (the steerTo fields alone; a site's other Out fields stay beside the call).
inline void SteerTo(Out& out, const float pos[3], float throttle, float fullAngle, int cruise)
{
    out.steerTo = true;
    memcpy(out.steerPos, pos, sizeof(out.steerPos));
    out.steerThrottle = throttle;
    out.steerFullAngle = fullAngle;
    out.cruise = cruise;
}

// The unstick rule (travel's ArriveAt, the fast dock's airlock run): while `until` lies ahead, back off (full reverse)
// sliding to side `dir`; else forward thrust wanted without forward speed (`pushing`, the site's own test) for more
// than `stuckFor` s starts a back-off of `backFor` s to a different side each time (the event names `who`).
inline void Unstick(Out& out, float now, float vFwd, bool pushing, float stuckFor, float backFor, float& stuckSince,
                    float& until, int& dir, const char* who)
{
    if (until > now)
    {
        out.steerThrottle = 0.0f;
        out.steerReverse = true;
        out.slide = dir;
        out.cruise = 0;
        out.afterburn = false;
    }
    else if (pushing)
    {
        if (stuckSince < 0)
            stuckSince = now;
        else if (now - stuckSince > stuckFor)
        {
            until = now + backFor;
            dir = 1 + (dir % 4); // a different side each time
            stuckSince = -1;
            events::Push("autopilot %s unstick dir=%d (forward %.0f m/s at full throttle)", who, dir, vFwd);
        }
    }
    else
        stuckSince = -1;
}

// The "charge not completing" watch (travel's ArriveAt, the combat hop): `since` = when the wanted charge began (-1
// none). While `charging` it runs; true once it has run more than `patience` s (the caller opens its afterburner
// window). Not charging and `clear`: back to -1.
inline bool ChargeFallback(float& since, bool charging, bool clear, float now, float patience)
{
    if (charging)
    {
        if (since < 0)
            since = now;
        else if (now - since > patience)
            return true;
    }
    else if (clear)
        since = -1;
    return false;
}

} // namespace autopilot
