// Shared-memory channel used by external test/debug tools (see tools\flctl.ps1).
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
};
#pragma pack(pop)

static const u32 kShmMagic = 0x54434C46; // 'FLCT'
static const u32 kShmVersion = 1;

namespace shm
{
bool Init();
ShmBlock* Get();
} // namespace shm
