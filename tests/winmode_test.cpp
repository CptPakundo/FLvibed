// Borderless windowed (core\winmode_rules.h): who provides it, what a display setup becomes while FLvibed does, the
// Alt+Enter toggle (FLUF flips a copy of the applied settings) and where the window goes.
#include "winmode_rules.h"

#include "test.h"

void Test_winmode()
{
    using namespace winmode;
    const unsigned vanillaStyle = 0x02CA0000u, hdeStyle = 0;
    // the provider
    CHECK(Decide(0, true, true, vanillaStyle, false) == Provider::Off);
    CHECK(Decide(0, false, false, 0, true) == Provider::Off);
    CHECK(Decide(1, false, true, vanillaStyle, false) == Provider::Unverified); // a hook refused: the game's mode
    CHECK(Decide(2, true, false, vanillaStyle, false) == Provider::Unverified); // the style site unverified
    CHECK(Decide(1, true, true, hdeStyle, true) == Provider::Game);             // HDE: its exe is borderless itself
    CHECK(Decide(1, true, true, vanillaStyle, true) == Provider::Wrapper);      // a bordered exe under d3d8to9 / dgVoodoo
    CHECK(Decide(1, true, true, vanillaStyle, false) == Provider::FLvibed);     // vanilla 1.1 (+ FLUF)
    CHECK(Decide(2, true, true, hdeStyle, true) == Provider::FLvibed);          // always
    CHECK(StyleIsBorderless(0x80000000u) && StyleIsBorderless(0) && !StyleIsBorderless(vanillaStyle) &&
          !StyleIsBorderless(0x00040000u));

    // the first setup (the game's start): windowed or fullscreen alike -> borderless at the monitor's size
    bool bordered = false;
    Setup s = Apply({true, 1280, 960}, false, false, bordered, 3840, 2160);
    CHECK(!s.out.fullscreen && s.borderless && !s.toggled && s.out.w == 3840 && s.out.h == 2160 && !bordered);
    s = Apply({false, 1280, 960}, true, false, bordered, 3840, 2160); // differs from applied, but not started: no toggle
    CHECK(!s.out.fullscreen && s.borderless && !s.toggled && !bordered);
    // the Options screen's resolution change: the same fullscreen byte as applied -> stays borderless
    s = Apply({false, 1600, 1200}, false, true, bordered, 3840, 2160);
    CHECK(!s.toggled && s.borderless && s.out.w == 3840 && s.out.h == 2160);
    // FLUF's Alt+Enter: applied windowed, the copy flipped to fullscreen -> the bordered window, never exclusive
    s = Apply({true, 3840, 2160}, false, true, bordered, 3840, 2160);
    CHECK(s.toggled && !s.borderless && !s.out.fullscreen && bordered && s.out.w == 3840);
    // again (the applied byte stays windowed): back to borderless
    s = Apply({true, 3840, 2160}, false, true, bordered, 3840, 2160);
    CHECK(s.toggled && s.borderless && !s.out.fullscreen && !bordered);
    // a game configured fullscreen (applied byte 1): FLUF's flip is a windowed request - still a toggle
    s = Apply({false, 1920, 1080}, true, true, bordered, 1920, 1080);
    CHECK(s.toggled && !s.borderless && bordered && s.out.w == 1920);
    bordered = false;
    // the monitor unknown: the request's size stays
    s = Apply({false, 1280, 960}, false, true, bordered, 0, 0);
    CHECK(s.borderless && s.out.w == 1280 && s.out.h == 960);

    // placement
    Rect r = Place(0, 0, 3840, 2160, 3840, 2160);
    CHECK(r.x == 0 && r.y == 0 && r.w == 3840 && r.h == 2160);
    r = Place(-1920, 0, 1920, 1080, 1280, 960); // a second monitor left of the primary, a smaller buffer: centred
    CHECK(r.x == -1920 + 320 && r.y == 60 && r.w == 1280 && r.h == 960);
}
