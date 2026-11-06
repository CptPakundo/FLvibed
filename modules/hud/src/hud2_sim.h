// HUD 2's debug states (`hud2 sim <name>[+comm] | list | off`, modules\hud\docs\UI_HUD2_PRESETS.md 6 step 2): the names and the
// parse, pure (tests\hud2_sim_test.cpp). What each state fills is hud2.cpp Simulate; the lint (`hud2 lint`) reads the
// frame each one draws.
#pragma once
#include <string>

namespace hud2sim
{
// The first five are the original mock-up states (unchanged); the rest are the presets' other contexts (UI_SPEC.json
// hud2_presets contexts: autopilot, turret, formation, jump, letterbox, comms; plus the 1.4 / 6 step 2 cases loot,
// capital, combat with no target, the target off screen). "comm" alone = a dock approach under a timer with the comm
// surface showing; "+comm" puts the comm surface over any of them (lane+comm, ap+comm).
inline constexpr const char* kNames[] = {"combat",    "travel", "cruise",    "dock",    "lane",     "ap",
                                         "apwait",    "turret", "formation", "jump",    "letterbox", "loot",
                                         "capital",   "notarget", "offscreen", "comm",   "torpedo",  "critical"};

struct Parsed
{
    bool ok = false;   // a known name (or off); false = refused, the current sim stays
    bool off = false;  // "", "off", "live": the live data again
    bool comm = false; // the comm surface forced on ("+comm", or the "comm" state itself)
    std::string base;  // the state's name ("" when off)
};

inline Parsed Parse(const std::string& s)
{
    Parsed p;
    if (s.empty() || s == "off" || s == "live")
    {
        p.ok = p.off = true;
        return p;
    }
    std::string base = s, mod;
    const size_t plus = s.find('+');
    if (plus != std::string::npos)
        base = s.substr(0, plus), mod = s.substr(plus + 1);
    bool known = false;
    for (const char* n : kNames)
        known = known || base == n;
    if (!known || (!mod.empty() && mod != "comm") || (plus != std::string::npos && mod.empty()))
        return p;
    p.ok = true;
    p.base = base;
    p.comm = mod == "comm" || base == "comm";
    return p;
}

// "combat|travel|..." for the command's answers
inline std::string Names()
{
    std::string s;
    for (const char* n : kNames)
        s += (s.empty() ? "" : "|") + std::string(n);
    return s;
}

// The state's name as `hud2 sim` / `hud2 state` print it ("-" when off)
inline std::string Label(const std::string& base, bool comm)
{
    if (base.empty())
        return "-";
    return base + (comm && base != "comm" ? "+comm" : "");
}
} // namespace hud2sim
