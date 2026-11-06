// HUD 2's debug states' names and parse (modules\hud\src\hud2_sim.h; `hud2 sim`, UI_HUD2_PRESETS.md 6 step 2):
// the original five unchanged, every new context accepted, +comm on any, unknown names and modifiers refused, off.
#include "../modules/hud/src/hud2_sim.h"

#include "test.h"

void Test_hud2_sim()
{
    using namespace hud2sim;
    // the original five and the new contexts
    for (const char* n : {"combat", "travel", "cruise", "dock", "lane", "ap", "apwait", "turret", "formation", "jump",
                          "letterbox", "loot", "capital", "notarget", "offscreen", "comm", "torpedo", "critical"})
    {
        const Parsed p = Parse(n);
        CHECK(p.ok && !p.off && p.base == n);
        CHECK(p.comm == (std::string(n) == "comm"));
    }
    CHECK(sizeof(kNames) / sizeof(kNames[0]) == 23);
    // +comm over any state
    {
        const Parsed p = Parse("lane+comm");
        CHECK(p.ok && p.base == "lane" && p.comm);
        CHECK(Label(p.base, p.comm) == "lane+comm");
        const Parsed q = Parse("ap+comm");
        CHECK(q.ok && q.base == "ap" && q.comm);
        CHECK(Label("comm", true) == "comm");
        CHECK(Label("", false) == "-");
    }
    // off
    for (const char* n : {"", "off", "live"})
    {
        const Parsed p = Parse(n);
        CHECK(p.ok && p.off && p.base.empty() && !p.comm);
    }
    // refused: unknown names / modifiers, a dangling +, case
    for (const char* n : {"calm", "Combat", "lane+", "lane+turret", "+comm", "comm+comm+comm", "list"})
        CHECK(!Parse(n).ok);
    // the list names every state
    const std::string names = Names();
    CHECK(names.compare(0, 13, "combat|travel") == 0);
    CHECK(names.find("|offscreen|comm") != std::string::npos);
}
