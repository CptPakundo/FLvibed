// The gamepad backend's pure pieces (modules\controller\src\pad_backend.h, WORK_QUEUE Q5 slice 1): which backend reads the pad,
// SDL's gamepad type -> the glyph family's vendor, SDL's positional buttons -> XInput's bits, SDL's axes -> XInput's.
#include "../modules/controller/src/pad_backend.h"

#include "test.h"

void Test_pad_backend()
{
    using namespace padbackend;

    // the backend: SDL only with pad_backend 1, a host that has it, no device-layer test pads, and SDL running
    CHECK(!WantSdl(0, true) && !WantSdl(1, false) && WantSdl(1, true));
    // the fallback: a PlayStation or Switch pad with no setting, never over an XInput pad
    CHECK(WantSdl(0, true, true) && !WantSdl(0, false, true));
    CHECK(ReadFromSdl(0, true, false, kSdlRunning, true) && ReadFromSdl(0, true, true, kSdlRunning, true)); // the fallback reads beside the test pads
    CHECK(!ReadFromSdl(1, true, true, kSdlRunning));                                                        // pad_backend 1 leaves them the pad
    CHECK(!ReadFromSdl(0, true, false, kSdlStarting, true));
    CHECK(HidCandidate(kVendorSony, 1, 5, false) && HidCandidate(kVendorNintendo, 1, 4, false));
    CHECK(!HidCandidate(kVendorSony, 1, 5, true));      // XInput reads this one itself
    CHECK(!HidCandidate(kVendorMicrosoft, 1, 5, false)); // an Xbox pad, and every other joystick: SDL would read nothing
    CHECK(!HidCandidate(0x044F, 1, 4, false));
    CHECK(!HidCandidate(kVendorSony, 1, 2, false));      // a mouse of theirs, a headset's page
    CHECK(!HidCandidate(kVendorSony, 12, 5, false));
    {
        Fallback f;
        const float dt = 0.5f;
        CHECK(!f.Want(true, true, true, false, dt)); // the wait: a plug-in that flaps starts nothing
        CHECK(!f.Want(true, true, true, false, dt));
        CHECK(!f.Want(true, true, true, false, dt));
        CHECK(f.Want(true, true, true, false, dt));
        CHECK(f.Want(true, true, true, false, dt));   // and it stays
        CHECK(!f.Want(true, true, false, false, dt)); // unplugged
        CHECK(!f.Want(true, true, true, false, dt));  // back: the wait again
        for (int i = 0; i < 20; ++i)
        {
            CHECK(!f.Want(false, true, true, false, dt)); // pad_sdl_fallback 0: the player asked for the mapper
            CHECK(!f.Want(true, false, true, false, dt)); // an older host
            CHECK(!f.Want(true, true, false, false, dt)); // no such pad: an Xbox or keyboard player, a Deck, never start SDL
        }
        // SDL that finds no pad stops and rests, then tries again: Steam hiding the pad from a game it started, so its virtual pad is the pad
        Fallback g;
        for (int i = 0; i < 4; ++i)
            g.Want(true, true, true, false, dt);
        CHECK(g.Want(true, true, true, false, dt));
        for (int i = 0; i < 19; ++i)
            CHECK(g.Want(true, true, true, true, dt));
        CHECK(!g.Want(true, true, true, true, dt));
        for (int i = 0; i < 119; ++i)
            CHECK(!g.Want(true, true, true, false, dt));
        for (int i = 0; i < 4; ++i)
            g.Want(true, true, true, false, dt);
        CHECK(g.Want(true, true, true, false, dt));
    }
    {
        // HasInput: the pad's own deadzones
        const int edge = static_cast<int>(0.24f * 32767);
        CHECK(!HasInput(0, 0, 0, 0, 0, 0, 0, 0.24f, 0.12f));
        CHECK(!HasInput(0, edge - 200, 0, 0, 0, 0, 0, 0.24f, 0.12f)); // drift inside the deadzone is rest
        CHECK(HasInput(0, edge + 200, 0, 0, 0, 0, 0, 0.24f, 0.12f));
        CHECK(!HasInput(0, 0, 0, 0, 0, 30, 0, 0.24f, 0.12f) && HasInput(0, 0, 0, 0, 0, 31, 0, 0.24f, 0.12f));
        CHECK(HasInput(0x1000, 0, 0, 0, 0, 0, 0, 0.24f, 0.12f));
        // Hands: frames of 10 ms
        const float dt = 0.01f;
        Hands h;
        CHECK(h.OnSdl(true, false, false, false, dt));   // the physical pad alone is the pad
        CHECK(h.OnSdl(true, true, true, true, dt));      // a press and its mirror: the physical pad, once
        for (int i = 0; i < 5; ++i)                      // released, the mirror lags 50 ms: no flip
            CHECK(h.OnSdl(true, false, true, true, dt));
        for (int i = 0; i < 100; ++i)                    // both at rest: it stays
            CHECK(h.OnSdl(true, false, true, false, dt));
        for (int i = 0; i < 3; ++i)                      // a mirror that leads by 30 ms never takes over
            CHECK(h.OnSdl(true, false, true, true, dt));
        CHECK(h.OnSdl(true, true, true, true, dt));
        for (int i = 0; i < 20; ++i)                     // the pad at rest: quiet starts over
            h.OnSdl(true, false, true, false, dt);
        bool over = false;                               // a real Xbox pad beside it: its own input, held
        for (int i = 0; i < 12; ++i)
            over = !h.OnSdl(true, false, true, true, dt);
        CHECK(over);
        for (int i = 0; i < 100; ++i)                    // and it stays while both rest: the prompts do not flip
            CHECK(!h.OnSdl(true, false, true, false, dt));
        CHECK(h.OnSdl(true, true, true, false, dt));     // the physical pad's press takes it back at once
        CHECK(!h.OnSdl(false, false, true, false, dt));  // SDL finds no pad: XInput's is the pad
        CHECK(h.OnSdl(true, false, false, false, dt));   // XInput's pad gone: SDL's
        // the injected rule
        CHECK(DropInjected(true, true, 0) && DropInjected(true, true, kInjectGraceMs - 1));
        CHECK(!DropInjected(true, true, kInjectGraceMs)); // another tool's key with the pad at rest passes
        CHECK(!DropInjected(true, false, 0));             // a real key or mouse is never touched
        CHECK(!DropInjected(false, true, 0));             // no pad in hand, or the game behind
    }
    CHECK(!ReadFromSdl(0, true, false, kSdlRunning)); // the default: XInput whatever the host does
    CHECK(ReadFromSdl(1, true, false, kSdlRunning));
    CHECK(!ReadFromSdl(1, false, false, kSdlRunning)); // an older host
    CHECK(!ReadFromSdl(1, true, true, kSdlRunning));   // the device-layer test pads are the XInput layer the tests drive
    CHECK(!ReadFromSdl(1, true, false, kSdlStarting)); // XInput while SDL_Init runs
    CHECK(!ReadFromSdl(1, true, false, kSdlFailed));   // fails closed to XInput
    CHECK(!ReadFromSdl(1, true, false, kSdlOff));
    CHECK(!ReadFromSdl(2, true, false, kSdlRunning)); // out of range (the registry clamps; never SDL by accident)

    // the glyph family's vendor from SDL's type (glyphs::Update: Sony -> PlayStation, Nintendo -> Nintendo, else Xbox)
    CHECK(VendorForType(kTypePs5, 0) == kVendorSony && VendorForType(kTypePs4, 0x1234) == kVendorSony &&
          VendorForType(kTypePs3, kVendorMicrosoft) == kVendorSony);
    CHECK(VendorForType(kTypeSwitchPro, 0) == kVendorNintendo && VendorForType(kTypeJoyconPair, 0) == kVendorNintendo &&
          VendorForType(kTypeJoyconLeft, 0) == kVendorNintendo && VendorForType(kTypeJoyconRight, 0) == kVendorNintendo &&
          VendorForType(kTypeGameCube, 0) == kVendorNintendo);
    CHECK(VendorForType(kTypeXboxOne, kVendorSony) == kVendorMicrosoft && VendorForType(kTypeXbox360, 0) == kVendorMicrosoft);
    // generic types: the pad's own vendor when it names a family, else the Xbox layout
    CHECK(VendorForType(kTypeStandard, kVendorSony) == kVendorSony && VendorForType(kTypeUnknown, kVendorNintendo) == kVendorNintendo);
    CHECK(VendorForType(kTypeStandard, 0x2DC8) == kVendorMicrosoft && VendorForType(kTypeUnknown, 0) == kVendorMicrosoft &&
          VendorForType(kTypeSteam, 0x28DE) == kVendorMicrosoft && VendorForType(99, 0) == kVendorMicrosoft);

    // the buttons: positional (south = A), each XInput bit once, the guide without one
    unsigned all = 0;
    int mapped = 0;
    for (int b = 0; b < kSdlButtons; ++b)
    {
        const unsigned x = kXiButtonForSdl[b];
        CHECK((all & x) == 0);
        all |= x;
        mapped += x != 0;
    }
    CHECK(mapped == 14 && all == 0xF3FF); // every XINPUT_GAMEPAD_* bit (0x0400 / 0x0800 are not XInput buttons)
    CHECK(kXiButtonForSdl[0] == 0x1000 && kXiButtonForSdl[1] == 0x2000 && kXiButtonForSdl[2] == 0x4000 &&
          kXiButtonForSdl[3] == 0x8000);                             // south A, east B, west X, north Y
    CHECK(kXiButtonForSdl[4] == 0x0020 && kXiButtonForSdl[6] == 0x0010); // back = View, start = Menu
    CHECK(kXiButtonForSdl[5] == 0);                                  // guide
    CHECK(kXiButtonForSdl[11] == 0x0001 && kXiButtonForSdl[14] == 0x0008); // D-pad up .. right

    // the axes: SDL's +y down -> XInput's +y up; the ends clamp; the triggers 0..32767 -> 0..255
    CHECK(StickX(0) == 0 && StickX(-32768) == -32768 && StickX(32767) == 32767 && StickX(40000) == 32767);
    CHECK(StickY(-32768) == 32767 && StickY(32767) == -32767 && StickY(0) == 0 && StickY(-16384) == 16384);
    CHECK(Trigger(0) == 0 && Trigger(-5) == 0 && Trigger(32767) == 255 && Trigger(16384) == 128 && Trigger(64) == 0 &&
          Trigger(129) == 1);

    // Impulse Triggers: Microsoft's own pad XInput reads; a Steam virtual pad, a Sony pad or a pad that is not XInput's are not it
    CHECK(XboxHid(kVendorXbox, 1, 5, true));
    CHECK(!XboxHid(0x28DE, 1, 5, true) && !XboxHid(kVendorSony, 1, 5, true) && !XboxHid(kVendorXbox, 1, 5, false) && !XboxHid(kVendorXbox, 1, 6, true));
    // the route: on only with everything for it, closed by each thing against it
    {
        const float dt = kImpulseLook;
        ImpulseRoute r;
        CHECK(r.Want(true, true, true, false, false, false, false, false, dt));
        CHECK(!r.Want(false, true, true, false, false, false, false, false, dt)); // `trigger_effects` off
        CHECK(!r.Want(true, false, true, false, false, false, false, false, dt)); // an older host
        CHECK(!r.Want(true, true, false, false, false, false, false, false, dt)); // no Xbox pad
        CHECK(!r.Want(true, true, true, true, false, false, false, false, dt));   // the fallback owns SDL
        CHECK(!r.Want(true, true, true, false, true, false, false, false, dt));   // Wine
        CHECK(!r.Want(true, true, true, false, false, true, false, false, dt));   // a test: never the machine's own pad
        // SDL up and no pad found (Windows 7, two pads): it gives up after kImpulseEmpty, and stays away until the pad comes back
        ImpulseRoute e;
        int looks = 0;
        while (e.Want(true, true, true, false, false, false, false, true, dt) && looks < 1000)
            ++looks;
        CHECK(looks == static_cast<int>(kImpulseEmpty / dt) - 1);
        CHECK(!e.Want(true, true, true, false, false, false, true, true, dt));
        CHECK(!e.Want(true, true, false, false, false, false, false, false, dt)); // the pad left
        CHECK(e.Want(true, true, true, false, false, false, false, false, dt));   // and came back
        // a pad found keeps it
        ImpulseRoute k;
        for (int i = 0; i < 100; ++i)
            CHECK(k.Want(true, true, true, false, false, false, true, true, dt));
    }
}
