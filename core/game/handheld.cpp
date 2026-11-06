// FLvibed - reads the machine for core\game\handheld.h: SMBIOS, Wine, the device form, the integrated panel. Every call is looked up at
// run time, so a Windows that lacks one leaves that fact out and the DLL still loads on Windows 7.
#include "game/handheld.h"

#include "safemem.h"

namespace handheld
{
namespace
{
void ReadSmbios(Facts& f)
{
    using Table = UINT(WINAPI*)(DWORD, DWORD, PVOID, DWORD);
    const auto table = reinterpret_cast<Table>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetSystemFirmwareTable"));
    if (!table)
        return;
    constexpr DWORD kRsmb = 'RSMB';
    const UINT need = table(kRsmb, 0, nullptr, 0);
    if (!need || need > (1u << 20))
        return;
    std::vector<unsigned char> buf(need);
    if (table(kRsmb, 0, buf.data(), need) == need)
        ParseSmbios(buf.data(), buf.size(), f);
}

void ReadFamily(Facts& f)
{
    using Family = void(WINAPI*)(unsigned long long*, DWORD*, DWORD*);
    const auto family = reinterpret_cast<Family>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetDeviceFamilyInfoEnum"));
    if (!family)
        return;
    DWORD device = 0, form = 0;
    family(nullptr, &device, &form);
    f.familyHandheld = form == 0x2E;
}

// QueryDisplayConfig's records as far as this file reads them (the SDK's own need a newer target).
struct PathInfo
{
    LUID sourceAdapter;
    UINT32 sourceId, sourceMode, sourceStatus;
    LUID targetAdapter;
    UINT32 targetId, targetMode, technology, rotation, scaling;
    UINT32 refreshNum, refreshDen, scanline;
    BOOL available;
    UINT32 targetStatus, flags;
};
struct ModeInfo
{
    unsigned char raw[64];
};
constexpr UINT32 kOnlyActive = 2, kInternal = 0x80000000u;

// The integrated panel's diagonal in inches from the monitor's reported size; `onlyPanel`: every active output is that panel.
void ReadPanel(Facts& f)
{
    using Sizes = LONG(WINAPI*)(UINT32, UINT32*, UINT32*);
    using Query = LONG(WINAPI*)(UINT32, UINT32*, PathInfo*, UINT32*, ModeInfo*, void*);
    HMODULE user = GetModuleHandleA("user32.dll");
    const auto sizes = reinterpret_cast<Sizes>(GetProcAddress(user, "GetDisplayConfigBufferSizes"));
    const auto query = reinterpret_cast<Query>(GetProcAddress(user, "QueryDisplayConfig"));
    if (!sizes || !query)
        return;
    UINT32 paths = 0, modes = 0;
    if (sizes(kOnlyActive, &paths, &modes) != 0 || !paths || paths > 64)
        return;
    std::vector<PathInfo> p(paths);
    std::vector<ModeInfo> m(modes ? modes : 1);
    if (query(kOnlyActive, &paths, p.data(), &modes, m.data(), nullptr) != 0)
        return;
    bool panel = false;
    for (UINT32 i = 0; i < paths; ++i)
        panel |= p[i].technology == kInternal;
    f.externalMonitor = !panel;
    if (!panel || paths != 1) // (an extended desktop: the device context below would measure the wrong screen)
        return;
    HDC dc = GetDC(nullptr);
    if (!dc)
        return;
    const float w = static_cast<float>(GetDeviceCaps(dc, HORZSIZE)), h = static_cast<float>(GetDeviceCaps(dc, VERTSIZE));
    ReleaseDC(nullptr, dc);
    if (w > 0 && h > 0)
        f.panelInches = sqrtf(w * w + h * h) / 25.4f;
}
} // namespace

Facts Read()
{
    Facts f;
    f.wine = GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version") != nullptr;
    char env[8] = {};
    f.steamDeckEnv = GetEnvironmentVariableA("SteamDeck", env, sizeof(env)) > 0 && env[0] == '1';
    ReadSmbios(f);
    ReadFamily(f);
    if (!f.wine) // (Wine's reports describe its own virtual desktop, and the rule that reads them is Windows')
        ReadPanel(f);
    return f;
}
} // namespace handheld
