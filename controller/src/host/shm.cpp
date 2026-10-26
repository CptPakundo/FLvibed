#include "../shm.h"

#include "log.h"

namespace shm
{
static HANDLE g_mapping = nullptr;
static ShmBlock* g_block = nullptr;

bool Init()
{
    // per game instance (parallel test games, 20 Oct 2026): FLVIBED_INSTANCE=<n> in the game's environment appends
    // "_<n>"; unset = the one classic name the harness opens by default
    std::string name = "Local\\FLControllerShm";
    const std::string inst = InstanceName();
    if (!inst.empty())
        name += "_" + inst;
    g_mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ShmBlock), name.c_str());
    if (!g_mapping)
    {
        Log("shm: CreateFileMapping failed (%lu)", GetLastError());
        return false;
    }
    g_block = static_cast<ShmBlock*>(MapViewOfFile(g_mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmBlock)));
    if (!g_block)
    {
        Log("shm: MapViewOfFile failed (%lu)", GetLastError());
        return false;
    }
    memset(g_block, 0, sizeof(ShmBlock));
    g_block->magic = kShmMagic;
    g_block->version = kShmVersion;
    return true;
}

ShmBlock* Get() { return g_block; }
} // namespace shm
