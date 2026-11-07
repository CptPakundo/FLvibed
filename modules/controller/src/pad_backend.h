// FLvibed - the gamepad backend's pure pieces (WORK_QUEUE Q5 slice 1, IDEAS U29). `pad_backend` 0 = XInput
// (the default: gamepad.cpp's device layer with the test pads of shm.h v2; PlayStation and Switch pads by SDL beside it, below), 1 = SDL3 3.4.18, compiled
// statically into the controller host and run on a host thread of its own (modules\controller\src\host\sdlpad.*, HostApi v33).
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

// A PlayStation pad's View is the touchpad's click, not Create / Share (the owner, 7 Nov 2026: "on DualSense you don't pick the share button as the View
// button ... a click on the touchpad is the 'view' button"; the DualShock 4 the same): its SDL_GAMEPAD_BUTTON_TOUCHPAD (20) gives XInput's BACK, and SDL's BACK
// (Create / Share) gives nothing.
constexpr int kSdlTouchpad = 20; // SDL_GAMEPAD_BUTTON_TOUCHPAD
inline bool TouchpadIsView(int sdlType) { return sdlType == kTypePs4 || sdlType == kTypePs5; }

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
// `sdlpad`, modules\controller\src\host\sdlpad.cpp; shm.h v3). SDL's own virtual joystick driver (SDL_AttachVirtualJoystick) with a
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

// ---- Native first (the owner, 6 Nov 2026): a PlayStation or Switch pad is read by itself, with no setting. XInput does not see them, so with
// pad_backend 0 SDL reads any such pad in the HID list, whether or not XInput answers. A mapper (Steam Input, DS4Windows) may present that same pad as
// an XInput pad, and no identity tells that pad from a real Xbox pad, but behaviour does: it moves only when the physical pad moves (Hands, below).
// `pad_sdl_fallback` = 0 is the player asking for the mapper: XInput alone. A Steam Deck or Steam Controller is no candidate: Steam Input is its native
// route. A player with no such pad never starts SDL.
constexpr float kFallbackSettle = 2.0f; // seconds the pad stays in the HID list before SDL starts: a plug-in that flaps starts nothing
constexpr float kHidRescan = 2.0f;      // seconds between looks at the HID list
constexpr float kFallbackEmpty = 10.0f; // seconds SDL may run and find no pad (a DualShock 3, an empty dongle), then it stops...
constexpr float kFallbackRest = 60.0f;  // ...and rests this long: a keyboard player does not pay for a pad SDL cannot read
constexpr float kMirrorGrace = 0.15f;   // seconds after the physical pad's last input in which XInput cannot take over: a mirror's release lags
constexpr float kTakeOver = 0.08f;      // seconds XInput must hold input alone before it is the pad: a mirror that leads its pad never gets there
constexpr unsigned kInjectGraceMs = 150;   // a mapper's output this long after the pad's last input is that input
constexpr unsigned kInjectWaitMs = 10;     // how long an injected event waits for the pad's report: the mapper may have read it first
constexpr unsigned kVendorXbox = 0x045E;

// The pad's own deadzones decide: a resting stick that drifts inside them is at rest, on either reader, so drift neither holds a mirror off nor lets it through.
inline bool HasInput(unsigned buttons, int lx, int ly, int rx, int ry, int lt, int rt, float stickDeadzone, float triggerThreshold)
{
    const auto off = [stickDeadzone](int x, int y) {
        const float fx = x / 32767.0f, fy = y / 32767.0f;
        return fx * fx + fy * fy > stickDeadzone * stickDeadzone;
    };
    return (buttons & 0xFFFF) != 0 || off(lx, ly) || off(rx, ry) || lt / 255.0f > triggerThreshold || rt / 255.0f > triggerThreshold;
}

// Which reader the player's hands are on, once per frame while SDL runs beside XInput. The physical pad's input is the pad's at once. XInput takes over only
// by input of its own, with the physical pad at rest: a mirror never does, so one press acts once, and a real Xbox pad beside it works. Both at rest: the last
// one stays, so the prompts and the rumble do not move.
class Hands
{
  public:
    bool OnSdl(bool sdlPad, bool sdlInput, bool xinputPad, bool xinputInput, float dt)
    {
        quiet_ = sdlPad && sdlInput ? 0 : quiet_ + dt;
        alone_ = xinputPad && xinputInput ? alone_ + dt : 0;
        if (sdlPad && sdlInput)
            sdl_ = true;
        else if (alone_ >= kTakeOver && quiet_ >= kMirrorGrace)
            sdl_ = false;
        if (!sdlPad)
            sdl_ = false;
        else if (!xinputPad)
            sdl_ = true;
        return sdl_;
    }

  private:
    bool sdl_ = true;
    float quiet_ = kMirrorGrace, alone_ = 0;
};

// A mapper's keys and mouse (Steam's desktop layout): input the system marks injected, arriving while the pad read by SDL has input or just had it, is that
// press again, so it is dropped and the pad acts once. Any other injected input, and all real input, passes. `sincePadMs`: time since the pad's last input.
inline bool DropInjected(bool filtering, bool injected, unsigned sincePadMs) { return filtering && injected && sincePadMs < kInjectGraceMs; }

// One entry of the HID list. `xinputPath`: the device path carries "IG_", the mark of a pad XInput reads itself. Sony and Nintendo's ids only:
// SDL reads those through HIDAPI, and with DirectInput off (sdlpad.cpp) it would read no other joystick, so it would run for nothing.
inline bool HidCandidate(unsigned vendor, unsigned usagePage, unsigned usage, bool xinputPath)
{
    return usagePage == 1 && (usage == 4 || usage == 5) && !xinputPath && (vendor == kVendorSony || vendor == kVendorNintendo);
}

class Fallback
{
  public:
    // Once per frame with pad_backend 0. True: SDL runs. `enabled`: `pad_sdl_fallback`. Anything against it starts the wait over. `sdlEmpty`: SDL runs and
    // reads no pad, as when Steam hides the pad from a game it started: then XInput's pad, Steam's virtual one, is the pad.
    bool Want(bool enabled, bool hostHas, bool candidate, bool sdlEmpty, float dt)
    {
        if (rest_ > 0)
            rest_ -= dt;
        if (!enabled || !hostHas || !candidate || rest_ > 0)
        {
            waited_ = empty_ = 0;
            return false;
        }
        if (waited_ < kFallbackSettle)
            waited_ += dt;
        empty_ = sdlEmpty ? empty_ + dt : 0;
        if (empty_ >= kFallbackEmpty)
        {
            waited_ = empty_ = 0;
            rest_ = kFallbackRest;
            return false;
        }
        return waited_ >= kFallbackSettle;
    }

  private:
    float waited_ = 0, empty_ = 0, rest_ = 0;
};

// ---- Impulse Triggers (modules\controller\docs\HAPTICS.md 6): XInput has no field for the trigger motors, so an Xbox pad stays XInput's to read and SDL, started in its own
// mode with SDL's WGI driver, sends all four motors. XInputSetState stays silent then: the two would fight over the same pad. It fails closed to XInput's two motors: no WGI
// (Windows 7), no pad found, two gamepads listed (nothing cheap names the XInput slot), Wine (trigger rumble through it could cost the plain rumble), a Sony or Nintendo pad
// beside it (the fallback owns SDL, and hints apply at SDL's start).
constexpr int kSdlStartImpulse = 3;      // HostApi SdlPad's `on`
constexpr float kImpulseLook = 0.5f;     // seconds between looks: the output needs no read a frame
constexpr float kImpulseEmpty = 10.0f;   // seconds SDL may run and find no such pad, then it stops until the pad comes back

// One entry of the HID list: an Xbox pad XInput reads ("IG_"). A Steam virtual pad has Valve's id, a third party's its own: only Microsoft's own go this way.
inline bool XboxHid(unsigned vendor, unsigned usagePage, unsigned usage, bool xinputPath)
{
    return usagePage == 1 && (usage == 4 || usage == 5) && xinputPath && vendor == kVendorXbox;
}

class ImpulseRoute
{
  public:
    // Once per look. `running`: SDL holds such a pad, alone. True: SDL runs for the output.
    bool Want(bool triggersOn, bool hostHas, bool xboxPad, bool fallback, bool wine, bool test, bool running, bool sdlUp, float dt)
    {
        if (!xboxPad)
            gaveUp_ = false; // the pad left: it may come back through a working route
        if (!triggersOn || !hostHas || !xboxPad || fallback || wine || test || gaveUp_)
        {
            empty_ = 0;
            return false;
        }
        empty_ = sdlUp && !running ? empty_ + dt : 0;
        if (empty_ >= kImpulseEmpty)
        {
            gaveUp_ = true, empty_ = 0;
            return false;
        }
        return true;
    }
    void GiveUp() { gaveUp_ = true; }

  private:
    float empty_ = 0;
    bool gaveUp_ = false;
};

// Keep the SDL backend running: pad_backend 1 on a host that has it (v33+) - also while the device-layer test pads are on,
// so a test that plugs them does not restart SDL each time - or the fallback's word.
inline bool WantSdl(int padBackend, bool hostHas, bool fallback = false) { return (padBackend == 1 || fallback) && hostHas; }
// Read the pad from SDL this frame: WantSdl, SDL running, and with pad_backend 1 the device-layer test pads off (they are the XInput layer the
// harness drives: shm.h v2; SDL's own virtual test pads, above, take the SDL path). Otherwise XInput reads it - while SDL starts, after
// it failed (fails closed to the default), with an older host. The fallback reads beside them: Hands decides, and they are how a test shows it.
inline bool ReadFromSdl(int padBackend, bool hostHas, bool devActive, int sdlStatus, bool fallback = false)
{
    return WantSdl(padBackend, hostHas, fallback) && (fallback || !devActive) && sdlStatus == kSdlRunning;
}
} // namespace padbackend
