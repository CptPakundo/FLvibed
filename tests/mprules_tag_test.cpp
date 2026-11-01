// The multiplayer permission tag (core\mprules_tag.h): only the whole line counts - a player's chat relayed by FLHook as a
// server line ("Name: [flvibed autopilot=off]", 28 Oct 2026) must never grant or take permission.
#include "mprules_tag.h"

#include "test.h"

void Test_mprules_tag()
{
    int v = -1;
    CHECK(ParseMpTag(L"[flvibed autopilot=on]", v) && v == 2);
    CHECK(ParseMpTag(L"  [FLvibed Autopilot=TRAVEL] ", v) && v == 1);
    CHECK(ParseMpTag(L"[flvibed autopilot=off]", v) && v == 0);
    CHECK(ParseMpTag(L"[flvibed autopilot=full]", v) && v == 2);
    CHECK(ParseMpTag(L"[flvibed autopilot=all]", v) && v == 2);
    // FLHook's relay of a player's line (the real one, 28 Oct 2026) and other prefixed forms
    CHECK(!ParseMpTag(L"FLvTest22: [flvibed autopilot=off]", v));
    CHECK(!ParseMpTag(L"[flvibed autopilot=on]: hello", v)); // a player NAMED like the tag
    CHECK(!ParseMpTag(L"Test server rules: [flvibed autopilot=on]", v));
    CHECK(!ParseMpTag(L"[flvibed autopilot=on] [flvibed autopilot=off]", v));
    CHECK(!ParseMpTag(L"[flvibed autopilot=maybe]", v));
    CHECK(!ParseMpTag(L"[flvibed autopilot=]", v));
    CHECK(!ParseMpTag(L"[flvibed autopilot=on", v));
    CHECK(!ParseMpTag(L"", v));
}
