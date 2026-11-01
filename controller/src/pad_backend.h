// FLvibed - the gamepad backend's pure pieces (WORK_QUEUE Q5 slice 1, IDEAS U29, 30 Oct 2026). `pad_backend` 0 = XInput
// (the default: gamepad.cpp's device layer with the test pads of shm.h v2, unchanged), 1 = SDL3 3.4.18, compiled
// statically into the controller host and run on a host thread of its own (controller\src\host\sdlpad.*, HostApi v33).
// Header only and free of system headers: the host includes it (and static_asserts the mirrored SDL values below against
// SDL's own enums), the logic includes it, and tests\pad_backend_test.cpp tests it host-free.
#pragma once

namespace padbackend
{
constexpr unsigned kHostVersion = 33; // HostApi v33: SdlPad / SdlPadRumble

// The SDL backend's state as the host reports it (SdlPadState::status, the return of HostApi::SdlPad).
constexpr int kSdlFailed = -1;  // SDL_Init failed (SdlPadState::error): XInput reads the pad until pad_backend goes 0 and 1 again
constexpr int kSdlOff = 0;      // not started (pad_backend 0) or stopped
constexpr int kSdlStarting = 1; // the host's thread runs SDL_Init (3-5 ms with DirectInput off, 30 Oct 2026; 115-126 ms
                                // with SDL's defaults, DirectInput's enumeration): XInput reads meanwhile
constexpr int kSdlRunning = 2;  // SDL reads the pad

// SDL3 3.4.18's SDL_GamepadType values (SDL_gamepad.h) - mirrored, checked against SDL's enum in sdlpad.cpp.
enum SdlType : int
{
    kTypeUnknown = 0,
    kTypeStandard,
    kTypeXbox360,
    kTypeXboxOne,
    kTypePs3,
    kTypePs4,
    kTypePs5,
    kTypeSwitchPro,
    kTypeJoyconLeft,
    kTypeJoyconRight,
    kTypeJoyconPair,
    kTypeGameCube,
    kTypeSteam,
    kTypeCount
};

constexpr unsigned kVendorMicrosoft = 0x045E, kVendorSony = 0x054C, kVendorNintendo = 0x057E;

// The vendor id the glyph family is chosen by (glyphs::Update's `vendor`: Sony = PlayStation, Nintendo = Nintendo, else
// Xbox) for a pad SDL names: SDL's type first (it knows a DualSense behind any vendor id, a Switch layout, a Steam
// controller), then the pad's own vendor id for the generic types, else Microsoft - the Xbox layout is the generic one.
inline unsigned VendorForType(int type, unsigned vendor)
{
    switch (type)
    {
    case kTypePs3:
    case kTypePs4:
    case kTypePs5: return kVendorSony;
    case kTypeSwitchPro:
    case kTypeJoyconLeft:
    case kTypeJoyconRight:
    case kTypeJoyconPair:
    case kTypeGameCube: return kVendorNintendo;
    case kTypeXbox360:
    case kTypeXboxOne: return kVendorMicrosoft;
    default: return vendor == kVendorSony || vendor == kVendorNintendo ? vendor : kVendorMicrosoft;
    }
}

// SDL's positional buttons (SDL_GamepadButton SOUTH .. DPAD_RIGHT = 0 .. 14) -> the XInput bits (XINPUT_GAMEPAD_*, the
// XInput ABI's values) every FLvibed reader uses: south = A wherever the label says B or cross (FLvibed's glyphs are
// positional too). The guide button has no XInput bit (Windows' Game Bar owns it): 0. The paddles, the touchpad and the
// misc buttons (15+) are not read in slice 1.
constexpr int kSdlButtons = 15;
constexpr unsigned kXiButtonForSdl[kSdlButtons] = {
    0x1000, // SOUTH          XINPUT_GAMEPAD_A
    0x2000, // EAST           B
    0x4000, // WEST           X
    0x8000, // NORTH          Y
    0x0020, // BACK           BACK (View)
    0,      // GUIDE          -
    0x0010, // START          START (Menu)
    0x0040, // LEFT_STICK     LEFT_THUMB
    0x0080, // RIGHT_STICK    RIGHT_THUMB
    0x0100, // LEFT_SHOULDER  LEFT_SHOULDER
    0x0200, // RIGHT_SHOULDER RIGHT_SHOULDER
    0x0001, // DPAD_UP
    0x0002, // DPAD_DOWN
    0x0004, // DPAD_LEFT
    0x0008, // DPAD_RIGHT
};

// SDL's stick axis -> XInput's: SDL's +y is down, XInput's up; -32768 (SDL full up) has no positive twin and clamps.
inline int StickX(int sdl) { return sdl < -32768 ? -32768 : sdl > 32767 ? 32767 : sdl; }
inline int StickY(int sdl)
{
    const int v = -StickX(sdl);
    return v > 32767 ? 32767 : v;
}
// SDL's trigger 0..32767 -> XInput's 0..255 (rounded; 32767 = 255).
inline int Trigger(int sdl) { return sdl <= 0 ? 0 : sdl >= 32767 ? 255 : (sdl * 255 + 16383) / 32767; }

// ---- The SDL virtual test pads (WORK_QUEUE Q5 slice 2, IDEAS T36 slice 2, 30 Oct 2026; TEST ONLY: the host's debug
// `sdlpad`, controller\src\host\sdlpad.cpp; shm.h v3). SDL's own virtual joystick driver (SDL_AttachVirtualJoystick) with a
// real pad's USB ids, so SDL names the type exactly as it does for the real pad (proven out of the game:
// research\evidence\sdl3\vpad.c) and the test pad takes the whole SDL path in the game: the type -> the glyph family,
// buttons, axes, triggers, hot-plug, and what the game sends to the device (rumble, trigger rumble, effects) read back.
struct TestPadKind
{
    const char* name;         // the harness's word: `sdlpad attach <name>`
    const char* sdlName;      // the device name SDL reports
    unsigned vendor, product; // the real pad's USB ids
    int touchpads;
    int type;           // the SdlType SDL names for these ids (a different one is reported by the attach)
    const char* family; // the glyph family it must select (glyphs::FamilyName; padhud `glyphs family=`)
};
constexpr int kTestPadKinds = 3;
constexpr TestPadKind kTestPads[kTestPadKinds] = {
    {"xbox", "FLvibed test pad (Xbox Series)", 0x045E, 0x0B12, 0, kTypeXboxOne, "xbox"},
    {"ps5", "FLvibed test pad (DualSense)", 0x054C, 0x0CE6, 1, kTypePs5, "playstation"},
    {"switchpro", "FLvibed test pad (Switch Pro)", 0x057E, 0x2009, 0, kTypeSwitchPro, "nintendo"},
};
constexpr int kTestPadSlots = 4; // shm.h v3 sdlSlot[]

// The kind by its harness word (`xbox`, `ps5`, `switchpro`; exact, lower case), -1 none.
inline int TestPadKindOf(const char* word)
{
    for (int k = 0; word && k < kTestPadKinds; ++k)
    {
        const char *a = word, *b = kTestPads[k].name;
        while (*a && *a == *b)
            ++a, ++b;
        if (!*a && !*b)
            return k;
    }
    return -1;
}

// The XInput form the harness writes (shm.h v3) -> the raw axis values the virtual device is set to, so the value read
// back through SDL's gamepad layer and StickX / StickY / Trigger above is the value written (tests\pad_backend_test.cpp).
// Sticks: SDL's gamepad axis is the raw axis (XInput's -32768 up has no SDL twin: it reads back as 32767 up). Triggers:
// SDL's virtual gamepad maps each trigger from a full raw axis (-32768 at rest .. 32767) onto 0..32767 (SDL3 3.4.18
// SDL_virtualjoystick.c: the triggers rest at SDL_JOYSTICK_AXIS_MIN, mapping "a4" / "a5"; SDL_gamepad.c's
// normalisation), so 0..255 goes to -32768 + 257 x value.
inline int SdlStickY(int xi)
{
    const int v = -xi;
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}
inline int SdlTriggerRaw(int xi) { return xi <= 0 ? -32768 : xi >= 255 ? 32767 : -32768 + 257 * xi; }
// SDL's gamepad value for a raw trigger axis (SDL_gamepad.c's float normalisation, mirrored for the unit test)
inline int SdlTriggerFromRaw(int raw)
{
    const float n = static_cast<float>(raw + 32768) / 65535;
    return static_cast<int>(n * 32767);
}
// SDL button b (0 .. kSdlButtons - 1) is down for XInput bits `xi` (the guide has no bit: never down).
inline bool SdlButtonDown(int b, unsigned xi) { return b >= 0 && b < kSdlButtons && (xi & kXiButtonForSdl[b]) != 0; }

// Which test pad the backend reads: while one is attached, the newest (the largest attach order; 0 = slot empty) - a
// real pad is ignored meanwhile, as the device-layer test pads make XInput ignore it. -1: none attached (SDL's first pad).
inline int NewestTestPad(const unsigned order[], int n)
{
    int best = -1;
    for (int i = 0; i < n; ++i)
        if (order[i] && (best < 0 || order[i] > order[best]))
            best = i;
    return best;
}

// Keep the SDL backend running: pad_backend 1 on a host that has it (v33+) - also while the device-layer test pads are on,
// so a test that plugs them does not restart SDL each time.
inline bool WantSdl(int padBackend, bool hostHas) { return padBackend == 1 && hostHas; }
// Read the pad from SDL this frame: WantSdl, SDL running, and the device-layer test pads off (they are the XInput layer the
// harness drives: shm.h v2; SDL's own virtual test pads, above, take the SDL path). Otherwise XInput reads it - while SDL starts, after
// it failed (fails closed to the default), with an older host.
inline bool ReadFromSdl(int padBackend, bool hostHas, bool devActive, int sdlStatus)
{
    return WantSdl(padBackend, hostHas) && !devActive && sdlStatus == kSdlRunning;
}
} // namespace padbackend
