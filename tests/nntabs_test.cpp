// core\game\nntabs.h (pure): the tab order, the wrap, and the press held through a slide.
#include "test.h"

#include "game/nntabs.h"

#include <cwchar>

using namespace nntabs;

void Test_nntabs()
{
    // the owner's words, in the game's own order (its top-right buttons, 7 Nov 2026); CHAT and GROUP join only in multiplayer
    CHECK(Count(false) == 5 && Count(true) == 7);
    const wchar_t* words[] = {L"LOG", L"MAP", L"INVENTORY", L"STATUS", L"INFO"};
    for (int i = 0; i < 5; ++i)
        CHECK(wcscmp(kTabs[i].word, words[i]) == 0);
    CHECK(IndexOf(kCmdChat, false) == -1 && IndexOf(kCmdChat, true) == 5);
    CHECK(IndexOf(0x6B, true) == -1); // (a dealer's room command is no tab)

    // the map between LOG and INVENTORY, as the game's buttons
    const int map = IndexOf(kCmdMap, false);
    CHECK(kTabs[Step(map, -1, 5)].cmd == kCmdLog && kTabs[Step(map, +1, 5)].cmd == kCmdInventory);
    // wrapping both ways
    CHECK(Step(0, -1, 5) == 4 && Step(4, +1, 5) == 0 && Step(6, +1, 7) == 0);
    CHECK(Step(0, 1, 0) == -1);

    // free: a press goes at once, the open window's own command never closes by a step
    Press p;
    CHECK(p.Go(kCmdLog, kCmdMap, false) == kCmdLog && !p.Held());
    CHECK(p.Go(kCmdMap, kCmdMap, false) == 0);

    // a slide: held, the last press wins, and it starts from the held tab
    CHECK(p.Go(kCmdInventory, kCmdMap, true) == 0 && p.Held());
    CHECK(p.Target(kCmdMap) == kCmdInventory);
    CHECK(p.Go(kCmdLog, kCmdMap, true) == 0 && p.cmd == kCmdLog);
    CHECK(p.Tick(0.016f, kCmdMap, true) == 0);
    CHECK(p.Tick(0.016f, kCmdMap, false) == kCmdLog && !p.Held());

    // pressed away and back during the slide: nothing is sent, the window would close
    CHECK(p.Go(kCmdStatus, kCmdMap, true) == 0 && p.Go(kCmdMap, kCmdMap, true) == 0);
    CHECK(p.Tick(0.016f, kCmdMap, false) == 0);

    // the bumpers pick: held while the steps come, sent once they stop for kSettleSeconds; stepping back to the open tab sends nothing
    p.Pick(kCmdInventory, kCmdMap);
    CHECK(p.Held() && p.Tick(0.2f, kCmdMap, false) == 0);
    p.Pick(kCmdStatus, kCmdMap); // (a second step restarts the wait)
    CHECK(p.Tick(0.3f, kCmdMap, false) == 0 && p.cmd == kCmdStatus);
    CHECK(p.Tick(0.2f, kCmdMap, false) == 0);
    CHECK(p.Tick(0.016f, kCmdMap, false) == kCmdStatus && !p.Held());
    p.Pick(kCmdLog, kCmdMap);
    p.Pick(kCmdMap, kCmdMap);
    CHECK(!p.Held() && p.Tick(1.0f, kCmdMap, false) == 0);

    // a hold that outlasts the slide's limit is dropped, never late
    CHECK(p.Go(kCmdLog, kCmdMap, true) == 0);
    for (float t = 0; t < kHoldSeconds + 0.1f; t += 0.1f)
        CHECK(p.Tick(0.1f, kCmdMap, true) == 0);
    CHECK(!p.Held() && p.Tick(0.1f, kCmdMap, false) == 0);

    // the close: the same command, held like any press; a window that went meanwhile is not reopened
    CHECK(p.Close(kCmdMap, false) == kCmdMap);
    CHECK(p.Close(0, false) == 0);
    CHECK(p.Close(kCmdMap, true) == 0 && p.close);
    CHECK(p.Tick(0.016f, kCmdMap, false) == kCmdMap);
    CHECK(p.Close(kCmdMap, true) == 0);
    CHECK(p.Tick(0.016f, 0, false) == 0);
}
