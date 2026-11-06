// The SDL virtual test pads' pure pieces (modules\controller\src\pad_backend.h, WORK_QUEUE Q5 slice 2, IDEAS T36 slice 2): the
// kinds (USB ids, the type SDL names, the glyph family they must select), the harness's XInput form -> the virtual
// device's raw values and back through SDL's gamepad layer (the trigger normalisation mirrored), the pad in use.
#include "../modules/controller/src/pad_backend.h"

#include "test.h"

void Test_sdl_testpad()
{
    using namespace padbackend;

    // the kinds by the harness's word; their ids are the real pads' (research\evidence\sdl3\vpad.c)
    CHECK(TestPadKindOf("xbox") == 0 && TestPadKindOf("ps5") == 1 && TestPadKindOf("switchpro") == 2);
    CHECK(TestPadKindOf("") == -1 && TestPadKindOf(nullptr) == -1 && TestPadKindOf("xbo") == -1 &&
          TestPadKindOf("xboxx") == -1 && TestPadKindOf("PS5") == -1);
    CHECK(kTestPads[0].vendor == 0x045E && kTestPads[0].product == 0x0B12 && kTestPads[0].type == kTypeXboxOne);
    CHECK(kTestPads[1].vendor == 0x054C && kTestPads[1].product == 0x0CE6 && kTestPads[1].type == kTypePs5 &&
          kTestPads[1].touchpads == 1);
    CHECK(kTestPads[2].vendor == 0x057E && kTestPads[2].product == 0x2009 && kTestPads[2].type == kTypeSwitchPro);
    // each kind selects its own glyph family through the type (glyphs::Update: Sony -> PlayStation, Nintendo -> Nintendo)
    CHECK(VendorForType(kTestPads[0].type, kTestPads[0].vendor) == kVendorMicrosoft);
    CHECK(VendorForType(kTestPads[1].type, kTestPads[1].vendor) == kVendorSony);
    CHECK(VendorForType(kTestPads[2].type, kTestPads[2].vendor) == kVendorNintendo);

    // sticks: X and Y round-trip through the raw axis (Y flips twice); XInput's -32768 has no SDL twin
    for (int v = -32767; v <= 32767; v += 7)
    {
        CHECK(StickX(v) == v);
        CHECK(StickY(SdlStickY(v)) == v);
    }
    CHECK(StickY(SdlStickY(32767)) == 32767 && StickY(SdlStickY(-32767)) == -32767);
    CHECK(SdlStickY(-32768) == 32767 && StickY(SdlStickY(-32768)) == -32767);
    CHECK(SdlStickY(0) == 0);

    // triggers: every XInput value 0..255 -> the raw axis -> SDL's 0..32767 -> back to the same value
    int exact = 0;
    for (int t = 0; t <= 255; ++t)
        exact += Trigger(SdlTriggerFromRaw(SdlTriggerRaw(t))) == t;
    CHECK(exact == 256);
    CHECK(SdlTriggerRaw(0) == -32768 && SdlTriggerRaw(-3) == -32768 && SdlTriggerRaw(255) == 32767 &&
          SdlTriggerRaw(300) == 32767);
    CHECK(SdlTriggerFromRaw(-32768) == 0 && SdlTriggerFromRaw(32767) == 32767); // at rest 0 (not half pressed)

    // buttons: every XInput bit sets exactly its SDL button and comes back as itself; the guide never
    for (int bit = 0; bit < 16; ++bit)
    {
        const unsigned xi = 1u << bit;
        unsigned back = 0;
        for (int b = 0; b < kSdlButtons; ++b)
            if (SdlButtonDown(b, xi))
                back |= kXiButtonForSdl[b];
        CHECK(back == (xi & 0xF3FF));
    }
    CHECK(!SdlButtonDown(5, 0xFFFF) && !SdlButtonDown(-1, 0xFFFF) && !SdlButtonDown(kSdlButtons, 0xFFFF));
    CHECK(SdlButtonDown(0, 0x1000) && !SdlButtonDown(0, 0x2000)); // south = A, positional

    // the pad in use: the newest attached test pad, none -1
    {
        const unsigned none[4] = {0, 0, 0, 0}, one[4] = {0, 0, 5, 0}, two[4] = {3, 0, 7, 0}, back[4] = {3, 0, 0, 0};
        CHECK(NewestTestPad(none, 4) == -1);
        CHECK(NewestTestPad(one, 4) == 2);
        CHECK(NewestTestPad(two, 4) == 2);  // attached after slot 0's
        CHECK(NewestTestPad(back, 4) == 0); // the newer one detached: the older one again
        CHECK(NewestTestPad(two, 0) == -1);
    }
}
