// Forwarders so the logic code can use the same shm API as the host.
#include "../shm.h"
#include "host.h"

const HostApi* g_host = nullptr;

namespace shm
{
bool Init() { return g_host && g_host->shm; }
ShmBlock* Get() { return g_host ? g_host->shm : nullptr; }
} // namespace shm
