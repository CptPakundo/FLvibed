// The gamepad backend's pure pieces (controller\src\pad_backend.h, WORK_QUEUE Q5 slice 1): which backend reads the pad,
// SDL's gamepad type -> the glyph family's vendor, SDL's positional buttons -> XInput's bits, SDL's axes -> XInput's.
#include "../controller/src/pad_backend.h"

#include "test.h"

void Test_pad_backend()
{
    using namespace padbackend;

    // the backend: SDL only with pad_backend 1, a host that has it, no device-layer test pads, and SDL running
    CHECK(!WantSdl(0, true) && !WantSdl(1, false) && WantSdl(1, true));
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
}
