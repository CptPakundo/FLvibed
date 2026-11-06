// Shared-memory channel used by external test/debug tools (see tools\harness\flctl.ps1).
// Lets a script feed a virtual gamepad and send debug commands to the running game.
#pragma once
#include "common.h"

#pragma pack(push, 4)
struct ShmBlock
{
    u32 magic; // 'FLCT'
    u32 version;
    u32 heartbeat; // incremented by the game every frame

    // Virtual gamepad (merged with the real controller when padActive != 0)
    u32 padActive;
    u32 padButtons;                 // XINPUT_GAMEPAD_* bits
    u32 padTriggers;                // low byte LT, next byte RT
    i32 padLX, padLY, padRX, padRY; // -32768..32767

    // Debug command mailbox
    u32 cmdSeq; // tool increments after writing cmd
    u32 cmdAck; // game copies cmdSeq here when the reply is ready
    char cmd[1024];
    char reply[60 * 1024];

    // ---- v2 (IDEAS T36 slice 1): the device-layer test pads. While devActive != 0 the XInput slots 0-3
    // are served from devSlot instead of the system's XInput (gamepad.cpp), so a test pad takes a real pad's whole path:
    // the slot scan, unplug / replug, the focus gate, the glyph family from its vendor, rumble. The virtual pad above
    // stays the focus-free harness hand, merged after the read. Read only when version >= 2 (a newer logic hot-reloaded
    // into an older host maps the v1 size).
    u32 devActive;
    u32 devFlags; // kDevForeground: the focus gate sees the game as in front (a background test of the open gate)
    struct DevSlot
    {
        u32 connected;                  // 0 = the slot answers ERROR_DEVICE_NOT_CONNECTED
        u32 buttons;                    // XINPUT_GAMEPAD_* bits
        u32 triggers;                   // low byte LT, next byte RT
        i32 lx, ly, rx, ry;             // -32768..32767
        u32 vendor;                     // the pad's USB vendor id: 0x045E Xbox, 0x054C PlayStation, 0x057E Nintendo
        u32 packet;                     // XINPUT_STATE.dwPacketNumber
    } devSlot[4];
    u32 devRumbleSeq; // rumble commands written so far; entry n is devRumble[n % 16]
    struct DevRumble
    {
        u32 seq;       // its number (1-based)
        u32 slot;      // the slot it went to
        u32 low, high; // the motor speeds, 0..65535
        u32 frame;     // the game's heartbeat when it was sent
    } devRumble[16];

    // ---- v3 (WORK_QUEUE Q5 slice 2, IDEAS T36 slice 2): the SDL virtual test pads (pad_backend 1; TEST ONLY,
    // the host's debug `sdlpad attach <xbox|ps5|switchpro> [slot]` / `detach`: modules\controller\src\host\sdlpad.cpp). The tool
    // writes each slot's state in the XInput form; the host's SDL thread sets it on the slot's SDL virtual device before
    // every read, so it comes back through SDL's whole gamepad layer; the host records what SDL sent to the device.
    // Read only when version >= 3.
    u32 sdlFlags;    // tool: kDevForeground - the focus gate sees the game as in front while an SDL test pad is attached
    u32 sdlAttached; // host: bit n = slot n attached
    struct SdlSlot
    {
        u32 buttons;        // tool: XINPUT_GAMEPAD_* bits
        u32 triggers;       // tool: low byte LT, next byte RT
        i32 lx, ly, rx, ry; // tool: -32768..32767, +y up (XInput's sign)
        u32 kind;           // host: pad_backend.h kTestPads index + 1 while attached, 0 empty
        u32 id;             // host: its SDL_JoystickID
    } sdlSlot[4];
    u32 sdlOutSeq; // host: device commands recorded so far; entry n is sdlOut[(n - 1) % 32]
    struct SdlOut
    {
        u32 seq;   // its number (1-based; written last)
        u32 slot;  // the test pad it went to
        u32 kind;  // kSdlOutRumble / kSdlOutTriggers / kSdlOutEffect
        u32 a, b;  // rumble: low / high motor 0..65535; trigger rumble: left / right 0..65535; effect: its bytes, 0
        u32 frame; // the game's heartbeat when SDL sent it
    } sdlOut[32];
};
#pragma pack(pop)

static const u32 kShmMagic = 0x54434C46; // 'FLCT'
static const u32 kShmVersion = 3;
static const u32 kDevForeground = 1;
static const u32 kSdlOutRumble = 1, kSdlOutTriggers = 2, kSdlOutEffect = 3;
// The tools write these offsets (tools\harness\flctl.core.ps1 Set-FLDevPad / Get-FLDevRumble): keep them in step.
static_assert(offsetof(ShmBlock, devActive) == 62512, "shm v2 layout: devActive");
static_assert(offsetof(ShmBlock, devSlot) == 62520 && sizeof(ShmBlock::DevSlot) == 36, "shm v2 layout: devSlot");
static_assert(offsetof(ShmBlock, devRumble) == 62668 && sizeof(ShmBlock::DevRumble) == 20, "shm v2 layout: devRumble");
// (flctl.core.ps1 Connect-FLSdlPad / Set-FLSdlPad / Get-FLSdlPadOut)
static_assert(offsetof(ShmBlock, sdlFlags) == 62988 && offsetof(ShmBlock, sdlAttached) == 62992, "shm v3 layout: sdlFlags");
static_assert(offsetof(ShmBlock, sdlSlot) == 62996 && sizeof(ShmBlock::SdlSlot) == 32, "shm v3 layout: sdlSlot");
static_assert(offsetof(ShmBlock, sdlOutSeq) == 63124 && offsetof(ShmBlock, sdlOut) == 63128 && sizeof(ShmBlock::SdlOut) == 24,
              "shm v3 layout: sdlOut");

namespace shm
{
bool Init();
ShmBlock* Get();
} // namespace shm
