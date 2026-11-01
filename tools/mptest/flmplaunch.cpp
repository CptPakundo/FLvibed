// flmplaunch.exe - TEST-ONLY launcher of the local multiplayer setup (tools\mptest\README.md); never shipped.
// FLServer.exe asks Common.dll for its data folder (GetUserDataPath) in its own start-up, before DACOM loads any
// [Libraries] entry of dacomsrv.ini - a plugin is too late (28 Oct 2026: an empty Documents\My Games\Freelancer
// appeared). So the launcher starts the program suspended, queues LoadLibraryA(flmptest.dll) as an APC on its main
// thread (it runs when the loader has mapped the static imports, before the program's entry point) and resumes it.
// Usage: flmplaunch.exe <dll> <working dir> <exe> [arguments...]; the program starts hidden; exit code 0 = started
// (prints the pid).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: flmplaunch <dll> <working dir> <exe> [args...]\n");
        return 2;
    }
    const char* dll = argv[1];
    std::string cmd = std::string("\"") + argv[3] + "\"";
    for (int i = 4; i < argc; ++i)
        cmd += std::string(" ") + argv[i];
    STARTUPINFOA si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(argv[3], &cmd[0], nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr, argv[2], &si, &pi))
    {
        fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        return 1;
    }
    const size_t n = strlen(dll) + 1;
    void* remote = VirtualAllocEx(pi.hProcess, nullptr, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    auto load = reinterpret_cast<PAPCFUNC>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA"));
    if (!remote || !WriteProcessMemory(pi.hProcess, remote, dll, n, nullptr) ||
        !QueueUserAPC(load, pi.hThread, reinterpret_cast<ULONG_PTR>(remote)))
    {
        fprintf(stderr, "injection failed: %lu\n", GetLastError());
        TerminateProcess(pi.hProcess, 1);
        return 1;
    }
    ResumeThread(pi.hThread);
    printf("%lu\n", pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
