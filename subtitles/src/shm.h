// Shared-memory debug channel used by tools\flsub.ps1 (commands to the running game, replies back).
#pragma once
#include "common.h"

#pragma pack(push, 4)
struct ShmBlock
{
    u32 magic; // 'FLSB'
    u32 version;
    u32 heartbeat; // incremented by the game every frame
    u32 cmdSeq;    // tool increments after writing cmd
    u32 cmdAck;    // game copies cmdSeq here when the reply is ready
    char cmd[1024];
    char reply[60 * 1024];
};
#pragma pack(pop)

static const u32 kShmMagic = 0x42534C46; // 'FLSB'
static const u32 kShmVersion = 1;

namespace shm
{
bool Init();
ShmBlock* Get();
} // namespace shm
