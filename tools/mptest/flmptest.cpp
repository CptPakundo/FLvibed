// flmptest.dll - TEST-ONLY helper for the local multiplayer test setup (tools\mptest\README.md). Never shipped, never
// in a player install: tools\mptest\mptest.ps1 lists it in a test instance's dacom.ini (client) and dacomsrv.ini
// (FLServer) only. The owner's decisions (28 Oct 2026): nothing of a multiplayer test may land outside the project.
//
// What it does, in every process that loads it:
// 1. The multiplayer account key. Common.dll keeps it in the registry, HKCU\Software\Microsoft\Microsoft Games\
//    Freelancer\1.0 values MPAccountName + MPAccountNameSig (the client creates them on first need and sends them to
//    the server; the same key a personal install of the game would use). Common.dll's imports of RegQueryValueExA /
//    RegSetValueExA are patched: those two values are read from / written to
//    <install>\SAVE\mptest\accounts\<set>\<value>.bin instead (<set> = FLVIBED_MPACCOUNT, default "default"; a missing
//    file reads as "not there", so the game makes a new key and writes it into the set). Every other registry access
//    passes through untouched.
// 2. In FLServer.exe only: Common.dll's GetUserDataPath (where FLServer.cfg and the server's accounts / characters
//    live; flplusplus redirects it in the client, nothing does in the server) returns <install>\SAVE\mptest\server.
// 3. In FLServer.exe with FLVIBED_BACKGROUND set: the server's windows are hidden as they appear (a test never takes
//    the screen).
// Every redirected access is logged to <install>\SAVE\mptest\mptest.log (time, process, what, which set), so a test
// always knows which account key it used and that the server's data path is not the game's default.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace
{
std::string g_root;    // <install>\SAVE\mptest
std::string g_set;     // the account set
std::string g_process; // exe file name
bool g_server = false;

void Log(const char* fmt, ...)
{
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    SYSTEMTIME t;
    GetLocalTime(&t);
    std::string path = g_root + "\\mptest.log";
    if (FILE* f = fopen(path.c_str(), "a"))
    {
        fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d.%03d %s[%lu] %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                t.wSecond, t.wMilliseconds, g_process.c_str(), GetCurrentProcessId(), line);
        fclose(f);
    }
}

void MakeDirs(const std::string& path)
{
    for (size_t i = 3; i <= path.size(); ++i)
        if (i == path.size() || path[i] == '\\')
            CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
}

// The values Common.dll keeps for multiplayer under HKCU\...\Freelancer\1.0: the account key and its signature, and the
// install's own key (InstallKey - written on the first multiplayer session; it reached the registry on 28 Oct 2026
// before this list had it; removed on the owner's word).
bool IsAccountValue(const char* name)
{
    return name &&
           (!_stricmp(name, "MPAccountName") || !_stricmp(name, "MPAccountNameSig") || !_stricmp(name, "InstallKey"));
}

std::string ValueFile(const char* name) { return g_root + "\\accounts\\" + g_set + "\\" + name + ".bin"; }

using QueryFn = LONG(WINAPI*)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD);
using SetFn = LONG(WINAPI*)(HKEY, LPCSTR, DWORD, DWORD, const BYTE*, DWORD);
QueryFn g_query = nullptr;
SetFn g_set_value = nullptr;

// File layout: u32 type, then the value's bytes.
LONG WINAPI QueryHook(HKEY key, LPCSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size)
{
    if (!IsAccountValue(name))
        return g_query(key, name, reserved, type, data, size);
    std::string path = ValueFile(name);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
    {
        Log("account set '%s': %s not there (the game makes a new key)", g_set.c_str(), name);
        return ERROR_FILE_NOT_FOUND;
    }
    DWORD t = 0;
    char buf[4096];
    size_t n = fread(&t, 1, 4, f) == 4 ? fread(buf, 1, sizeof buf, f) : 0;
    fclose(f);
    if (type)
        *type = t;
    LONG r = ERROR_SUCCESS;
    if (data)
    {
        if (!size || *size < n)
            r = ERROR_MORE_DATA;
        else
            memcpy(data, buf, n);
    }
    if (size)
        *size = static_cast<DWORD>(n);
    Log("account set '%s': read %s (%u bytes)", g_set.c_str(), name, static_cast<unsigned>(n));
    return r;
}

LONG WINAPI SetHook(HKEY key, LPCSTR name, DWORD reserved, DWORD type, const BYTE* data, DWORD size)
{
    if (!IsAccountValue(name))
        return g_set_value(key, name, reserved, type, data, size);
    std::string path = ValueFile(name);
    MakeDirs(g_root + "\\accounts\\" + g_set);
    FILE* f = fopen(path.c_str(), "wb");
    if (!f)
        return ERROR_ACCESS_DENIED;
    fwrite(&type, 1, 4, f);
    if (data && size)
        fwrite(data, 1, size, f);
    fclose(f);
    Log("account set '%s': wrote %s (%lu bytes; NOT the registry)", g_set.c_str(), name, size);
    return ERROR_SUCCESS;
}

// Replaces an imported function's IAT entry in `module`; returns the previous pointer (null when not imported).
void* PatchImport(HMODULE module, const char* dll, const char* func, void* hook)
{
    auto base = reinterpret_cast<BYTE*>(module);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return nullptr;
    for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp)
    {
        if (_stricmp(reinterpret_cast<char*>(base + imp->Name), dll))
            continue;
        auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk
                                                                                          : imp->FirstThunk));
        auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots)
        {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                continue;
            auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (strcmp(reinterpret_cast<char*>(byName->Name), func))
                continue;
            void* prev = reinterpret_cast<void*>(slots->u1.Function);
            DWORD old;
            VirtualProtect(&slots->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            slots->u1.Function = reinterpret_cast<ULONG_PTR>(hook);
            VirtualProtect(&slots->u1.Function, sizeof(void*), old, &old);
            return prev;
        }
    }
    return nullptr;
}

// FLServer's data folder (Common.dll bool GetUserDataPath(char* const)).
bool __cdecl UserDataPath(char* const out)
{
    std::string dir = g_root + "\\server";
    MakeDirs(dir);
    strcpy_s(out, MAX_PATH, dir.c_str());
    static bool logged = false;
    if (!logged)
    {
        logged = true;
        Log("server data path -> %s (NOT the game's default Documents\\My Games\\Freelancer)", dir.c_str());
    }
    return true;
}

// Common.dll builds "<Documents>\My Games\Freelancer" in more than one place (an empty folder still appeared with
// GetUserDataPath detoured, 28 Oct 2026): in the server its SHGetFolderPathA answers <root>\server\Documents for
// CSIDL_PERSONAL, so whatever it makes stays in the instance.
using FolderFn = HRESULT(WINAPI*)(HWND, int, HANDLE, DWORD, LPSTR);
FolderFn g_folder = nullptr;
HRESULT WINAPI FolderHook(HWND w, int csidl, HANDLE token, DWORD flags, LPSTR out)
{
    if ((csidl & 0xFF) != 0x05 /* CSIDL_PERSONAL */)
        return g_folder(w, csidl, token, flags, out);
    std::string dir = g_root + "\\server\\Documents";
    MakeDirs(dir);
    strcpy_s(out, MAX_PATH, dir.c_str());
    Log("server: Common.dll asked for Documents -> %s", dir.c_str());
    return S_OK;
}

void Detour(void* from, void* to)
{
    DWORD old;
    auto p = static_cast<BYTE*>(from);
    VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    *reinterpret_cast<int*>(p + 1) = static_cast<int>(static_cast<BYTE*>(to) - p - 5);
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
}

void CALLBACK OnShow(HWINEVENTHOOK, DWORD, HWND hwnd, LONG obj, LONG, DWORD, DWORD)
{
    // every top-level window, owned ones too (the "Server Configuration" dialog is owned by the main frame)
    if (obj == OBJID_WINDOW && hwnd && !(GetWindowLongA(hwnd, GWL_STYLE) & WS_CHILD) && IsWindowVisible(hwnd))
    {
        ShowWindow(hwnd, SW_HIDE);
        char title[128] = "";
        GetWindowTextA(hwnd, title, sizeof title);
        Log("server: window '%s' hidden", title);
    }
}

DWORD WINAPI HideThread(void*)
{
    HWINEVENTHOOK h =
        SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, nullptr, OnShow, GetCurrentProcessId(), 0,
                        WINEVENT_OUTOFCONTEXT);
    Log("server: hiding its windows (background)%s", h ? "" : " - SetWinEventHook FAILED");
    // windows shown before the hook
    EnumWindows(
        [](HWND w, LPARAM) -> BOOL {
            DWORD pid = 0;
            GetWindowThreadProcessId(w, &pid);
            if (pid == GetCurrentProcessId() && IsWindowVisible(w))
                ShowWindow(w, SW_HIDE);
            return TRUE;
        },
        0);
    MSG m;
    while (GetMessageA(&m, nullptr, 0, 0) > 0)
        DispatchMessageA(&m);
    if (h)
        UnhookWinEvent(h);
    return 0;
}

void Start()
{
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string path = exe;
    size_t slash = path.find_last_of('\\');
    g_process = path.substr(slash + 1);
    std::string exeDir = path.substr(0, slash);
    g_root = exeDir.substr(0, exeDir.find_last_of('\\')) + "\\SAVE\\mptest";
    MakeDirs(g_root);
    char set[64] = "default";
    GetEnvironmentVariableA("FLVIBED_MPACCOUNT", set, sizeof set);
    g_set = set;
    g_server = !_stricmp(g_process.c_str(), "FLServer.exe");

    HMODULE common = GetModuleHandleA("common.dll");
    if (!common)
    {
        Log("no common.dll: nothing patched");
        return;
    }
    g_query = reinterpret_cast<QueryFn>(PatchImport(common, "ADVAPI32.dll", "RegQueryValueExA",
                                                    reinterpret_cast<void*>(QueryHook)));
    g_set_value =
        reinterpret_cast<SetFn>(PatchImport(common, "ADVAPI32.dll", "RegSetValueExA", reinterpret_cast<void*>(SetHook)));
    Log("loaded; account set '%s'; Common.dll registry imports %s / %s", g_set.c_str(), g_query ? "patched" : "MISSING",
        g_set_value ? "patched" : "MISSING");
    if (g_server)
    {
        if (void* f = GetProcAddress(common, "?GetUserDataPath@@YA_NQAD@Z"))
        {
            Detour(f, reinterpret_cast<void*>(UserDataPath));
            Log("server: GetUserDataPath redirected");
        }
        else
            Log("server: GetUserDataPath NOT FOUND - the server would use Documents");
        g_folder = reinterpret_cast<FolderFn>(
            PatchImport(common, "SHFOLDER.dll", "SHGetFolderPathA", reinterpret_cast<void*>(FolderHook)));
        Log("server: Common.dll SHGetFolderPathA %s", g_folder ? "redirected" : "NOT IMPORTED");
        char bg[8];
        if (GetEnvironmentVariableA("FLVIBED_BACKGROUND", bg, sizeof bg))
        {
            // FLHook (HDE's server plugin) opens a console: give the process a windowless one first, so its
            // AllocConsole finds one and nothing appears (Windows 11 24H2+ AllocConsoleWithOptions NO_WINDOW; older:
            // a console hidden at once)
            struct Options
            {
                int mode;
                BOOL useShowWindow;
                WORD showWindow;
            } opt{2 /* ALLOC_CONSOLE_MODE_NO_WINDOW */, FALSE, 0};
            using AllocFn = HRESULT(WINAPI*)(const Options*, int*);
            auto alloc = reinterpret_cast<AllocFn>(
                GetProcAddress(GetModuleHandleA("kernel32.dll"), "AllocConsoleWithOptions"));
            int result = 0;
            if (alloc && SUCCEEDED(alloc(&opt, &result)))
                Log("server: windowless console made (FLHook's console stays hidden)");
            else if (AllocConsole())
            {
                ShowWindow(GetConsoleWindow(), SW_HIDE);
                Log("server: console made and hidden (no AllocConsoleWithOptions)");
            }
            CloseHandle(CreateThread(nullptr, 0, HideThread, nullptr, 0, nullptr));
        }
    }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        Start();
    }
    return TRUE;
}
