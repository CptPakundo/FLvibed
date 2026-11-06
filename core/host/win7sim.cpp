// FLvibed - the Windows 7 simulation's run-time half (FLVIBED_SIM_WIN7, common.h): FLvibed.dll's own imports of GetProcAddress and
// LoadLibrary answer as Windows 7 would, so every run-time look-up in the DLL takes its fallback here: FLvibed's, SDL's and the C
// runtime's. tools\win7_check.ps1 fails when the binary names a Windows API that real Windows 7 lacks and kNewerThanWindows7 does not list.
// The C runtime's look-ups made before the DLL's entry reaches Install keep what they found. A release build: in a dev build the logic is a
// DLL of its own, whose imports this does not patch.
#include "all.h"

#include "win7sim.h"

namespace win7sim
{
// Every name of a current system DLL that the binary holds as text and the real Windows 7 SP1 files do not export.
static const char* const kNewerThanWindows7[] = {
    "AppPolicyGetProcessTerminationMethod",
    "AppPolicyGetThreadInitializationType",
    "CM_Register_Notification",
    "CM_Unregister_Notification",
    "CoIncrementMTAUsage",
    "FlsGetValue2",
    "GetAwarenessFromDpiAwarenessContext",
    "GetDpiForWindow",
    "GetSystemTimePreciseAsFileTime",
    "GetThreadDpiAwarenessContext",
    "RtlGetDeviceFamilyInfoEnum",
    "RoGetActivationFactory",
    "RoInitialize",
    "RoUninitialize",
    "SetThreadDescription",
    "WaitOnAddress",
    "WakeByAddressSingle",
    "WindowsCreateStringReference",
    "WindowsDeleteString",
    "WindowsGetStringRawBuffer",
};

// DLLs Windows 7 does not have. Every API set counts: the few it has export nothing the look-ups above would find.
static bool MissingDll(const char* name)
{
    const char* leaf = strrchr(name, '\\');
    leaf = leaf ? leaf + 1 : name;
    return _strnicmp(leaf, "api-ms-", 7) == 0 || _strnicmp(leaf, "ext-ms-", 7) == 0 || _stricmp(leaf, "combase.dll") == 0 ||
           _stricmp(leaf, "gameinput.dll") == 0 || _stricmp(leaf, "xinput1_4.dll") == 0;
}

static LONG g_refused = 0;

static void Refused(const char* what)
{
    if (InterlockedIncrement(&g_refused) <= 40)
        Log("win7 sim: refused %s", what);
    SetLastError(ERROR_PROC_NOT_FOUND);
}

// The real functions: a call by name in this file would go through the import slot Install patches.
static decltype(&GetProcAddress) g_getProc = nullptr;
static decltype(&LoadLibraryA) g_loadA = nullptr;
static decltype(&LoadLibraryW) g_loadW = nullptr;
static decltype(&LoadLibraryExA) g_loadExA = nullptr;
static decltype(&LoadLibraryExW) g_loadExW = nullptr;

static FARPROC WINAPI GetProcAddressSim(HMODULE module, LPCSTR name)
{
    if (reinterpret_cast<uintptr_t>(name) > 0xFFFF) // (a low word is an ordinal)
        for (const char* newer : kNewerThanWindows7)
            if (strcmp(name, newer) == 0)
            {
                Refused(name);
                return nullptr;
            }
    return g_getProc(module, name);
}

static bool Refuse(const char* name)
{
    if (!name || !MissingDll(name))
        return false;
    Refused(name);
    SetLastError(ERROR_MOD_NOT_FOUND);
    return true;
}

static bool RefuseW(const wchar_t* name)
{
    char narrow[MAX_PATH] = {};
    return name && WideCharToMultiByte(CP_ACP, 0, name, -1, narrow, sizeof(narrow), nullptr, nullptr) && Refuse(narrow);
}

static HMODULE WINAPI LoadLibraryASim(LPCSTR name) { return Refuse(name) ? nullptr : g_loadA(name); }
static HMODULE WINAPI LoadLibraryWSim(LPCWSTR name) { return RefuseW(name) ? nullptr : g_loadW(name); }
static HMODULE WINAPI LoadLibraryExASim(LPCSTR name, HANDLE file, DWORD flags)
{
    return Refuse(name) ? nullptr : g_loadExA(name, file, flags);
}
static HMODULE WINAPI LoadLibraryExWSim(LPCWSTR name, HANDLE file, DWORD flags)
{
    return RefuseW(name) ? nullptr : g_loadExW(name, file, flags);
}

void Install(HMODULE self)
{
    if (!SimWin7())
        return;
    g_getProc = GetProcAddress, g_loadA = LoadLibraryA, g_loadW = LoadLibraryW, g_loadExA = LoadLibraryExA, g_loadExW = LoadLibraryExW;
    int patched = 0;
    const struct
    {
        const char* name;
        void* fn;
    } imports[] = {{"GetProcAddress", reinterpret_cast<void*>(GetProcAddressSim)},
                   {"LoadLibraryA", reinterpret_cast<void*>(LoadLibraryASim)},
                   {"LoadLibraryW", reinterpret_cast<void*>(LoadLibraryWSim)},
                   {"LoadLibraryExA", reinterpret_cast<void*>(LoadLibraryExASim)},
                   {"LoadLibraryExW", reinterpret_cast<void*>(LoadLibraryExWSim)}};
    for (const auto& i : imports)
        patched += hk::PatchImport(self, "kernel32.dll", i.name, i.fn) != nullptr;
    Log("win7 sim: %d of 5 imports answer as Windows 7", patched);
}
} // namespace win7sim
