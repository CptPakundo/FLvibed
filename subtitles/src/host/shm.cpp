#include "../shm.h"

#include "log.h"

namespace shm
{
static ShmBlock* g_block = nullptr;

bool Init()
{
    // per game instance (parallel test games): FLVIBED_INSTANCE=<n> appends "_<n>"; unset = the classic name
    std::string name = "Local\\FLSubtitlesShm";
    const std::string inst = InstanceName();
    if (!inst.empty())
        name += "_" + inst;
    HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(ShmBlock), name.c_str());
    if (!mapping)
    {
        Log("shm: CreateFileMapping failed (%lu)", GetLastError());
        return false;
    }
    g_block = static_cast<ShmBlock*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ShmBlock)));
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
