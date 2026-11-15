// FLvibed - the crash recorder (crashlog.h): the process's top-level exception filter, in front of the game's, writing one
// block per crash to EXE\FLvibed_crash.log; on a test start (FLVIBED_TEST=1) also a minidump beside it.
// The filter runs on the faulting thread with the process in an unknown state, so it allocates nothing, takes no lock of
// the mod's, the C runtime's or the loader's (no Log(), no printf, no GetModuleHandle, no LoadLibrary), reads memory only
// under SEH and leaves the outcome to the filter it replaced: the game's ends the process with the exception code.
#include "all.h"

#include "crashlog.h"
#include "crashlog_rules.h"

namespace crashlog
{
using crashrules::Text;

typedef DWORD(WINAPI* MappedNameFn)(HANDLE, LPVOID, LPWSTR, DWORD);       // K32GetMappedFileNameW (no loader lock)
typedef LONG(NTAPI* ThreadInfoFn)(HANDLE, ULONG, PVOID, ULONG, PULONG); // NtQueryInformationThread
typedef BOOL(WINAPI* MiniDumpFn)(HANDLE, DWORD, HANDLE, int, const void*, const void*, const void*);
#pragma pack(push, 4)
struct DumpExceptionInfo // MINIDUMP_EXCEPTION_INFORMATION (dbghelp.h, not included: it would land inside the namespace)
{
    DWORD threadId;
    EXCEPTION_POINTERS* pointers;
    BOOL clientPointers;
};
#pragma pack(pop)

static LPTOP_LEVEL_EXCEPTION_FILTER g_prev = nullptr; // the filter called after the block (the game's)
static bool g_installed = false;
static volatile LONG g_taken = 0; // the first crash only: a second thread's crash meanwhile goes straight on
static MappedNameFn g_mappedName = nullptr;
static ThreadInfoFn g_threadInfo = nullptr;
static MiniDumpFn g_miniDump = nullptr; // test starts only
static DWORD g_mainThread = 0;          // the thread that loaded FLvibed: the game's main thread
static uintptr_t g_selfBase = 0;        // FLvibed.dll's base, to read its offsets against the build's map
static wchar_t g_logPath[MAX_PATH];
static wchar_t g_dumpPath[MAX_PATH];
static char g_block[16384]; // static: a stack overflow leaves the faulting thread almost no stack
static DumpExceptionInfo g_dumpInfo;

static const char* CodeName(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "misalignment";
    case 0xC0000029: return "invalid unwind target";
    case 0xC0000374: return "heap corruption";
    case 0xC0000409: return "stack buffer overrun";
    case 0xE06D7363: return "C++ exception";
    default: return "exception";
    }
}

// The module an address lies in: its base and file name (the mapped file's last component). False: not committed memory.
static bool ModuleOf(uintptr_t addr, uintptr_t& base, char* name, size_t cap, bool& exec)
{
    MEMORY_BASIC_INFORMATION m;
    if (!VirtualQuery(reinterpret_cast<void*>(addr), &m, sizeof(m)) || m.State != MEM_COMMIT)
        return false;
    exec = (m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    base = m.Type == MEM_IMAGE ? reinterpret_cast<uintptr_t>(m.AllocationBase) : 0;
    name[0] = '\0';
    if (!base)
        return true;
    wchar_t w[MAX_PATH];
    const DWORD n = g_mappedName ? g_mappedName(GetCurrentProcess(), m.AllocationBase, w, MAX_PATH) : 0;
    DWORD from = 0;
    for (DWORD i = 0; i < n; ++i)
        if (w[i] == L'\\')
            from = i + 1;
    size_t k = 0;
    for (DWORD i = from; i < n && k + 1 < cap; ++i)
        name[k++] = w[i] < 128 ? static_cast<char>(w[i]) : '?';
    name[k] = '\0';
    if (!k && cap > 1)
        name[0] = '?', name[1] = '\0';
    return true;
}

// "Module.dll+0x0001A2B3", "0x12345678 (not in a module)" or "0x12345678 (unmapped)"
static void Where(Text& t, uintptr_t addr)
{
    uintptr_t base = 0;
    bool exec = false;
    char name[96];
    if (!ModuleOf(addr, base, name, sizeof(name), exec))
        t.Hex(static_cast<uint32_t>(addr)).Put(" (unmapped)");
    else if (!base)
        t.Hex(static_cast<uint32_t>(addr)).Put(" (not in a module)");
    else
        t.Put(name).Char('+').Hex(static_cast<uint32_t>(addr - base));
}

// A stack value that points just behind a call in a module's code.
static bool IsReturnAddress(uintptr_t v)
{
    if (v < 0x10000)
        return false;
    uintptr_t base = 0;
    bool exec = false;
    char name[4];
    if (!ModuleOf(v, base, name, sizeof(name), exec) || !base || !exec || v - base < 8)
        return false;
    uint8_t before[7];
    __try
    {
        memcpy(before, reinterpret_cast<const void*>(v - 7), 7);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return crashrules::LooksLikeReturn(before, 7);
}

static void WriteOut(const char* text, size_t len)
{
    HANDLE f = CreateFileW(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(f, text, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(f);
}

static void Record(EXCEPTION_POINTERS* ep, const char* kind)
{
    Text t(g_block, sizeof(g_block));
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    const CONTEXT* c = ep->ContextRecord;
    SYSTEMTIME st;
    GetLocalTime(&st);
    t.Put("==== FLvibed crash recorder: ").Put(kind).Put(", ").Dec(st.wYear).Char('-').Dec2(st.wMonth).Char('-').Dec2(st.wDay);
    t.Char(' ').Dec2(st.wHour).Char(':').Dec2(st.wMinute).Char(':').Dec2(st.wSecond).Char('.');
    t.Char(static_cast<char>('0' + st.wMilliseconds / 100)).Dec2(st.wMilliseconds % 100);
    t.Put(" pid ").Dec(GetCurrentProcessId()).Put("\r\nbuild ").Put(__DATE__ " " __TIME__);
#ifdef FLV_STATIC
    t.Put(" (release)");
#else
    t.Put(" (dev)");
#endif
    t.Put(", FLvibed.dll at ").Hex(static_cast<uint32_t>(g_selfBase));
    t.Put("\r\ncode ").Hex(er->ExceptionCode).Put(" (").Put(CodeName(er->ExceptionCode)).Put(") at ");
    Where(t, reinterpret_cast<uintptr_t>(er->ExceptionAddress));
    if (er->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && er->NumberParameters >= 2)
    {
        t.Put(", ").Put(crashrules::AccessKind(static_cast<uint32_t>(er->ExceptionInformation[0]))).Put(" of ");
        Where(t, er->ExceptionInformation[1]);
    }
    const DWORD tid = GetCurrentThreadId();
    t.Put("\r\nthread ").Dec(tid).Put(tid == g_mainThread ? " (the game's main thread)" : "");
    uintptr_t start = 0;
    if (g_threadInfo && g_threadInfo(GetCurrentThread(), 9 /* ThreadQuerySetWin32StartAddress */, &start, sizeof(start),
                                     nullptr) == 0 && start)
    {
        t.Put(", started at ");
        Where(t, start);
    }
    t.Put("\r\nregisters eip ").Hex(c->Eip).Put(" esp ").Hex(c->Esp).Put(" ebp ").Hex(c->Ebp).Put(" eax ").Hex(c->Eax);
    t.Put(" ebx ").Hex(c->Ebx).Put(" ecx ").Hex(c->Ecx).Put(" edx ").Hex(c->Edx).Put(" esi ").Hex(c->Esi).Put(" edi ").Hex(c->Edi);

    // The faulting thread's stack is this thread's (a filter runs on it): its bounds from the TEB.
    const NT_TIB* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
    const uintptr_t lo = reinterpret_cast<uintptr_t>(tib->StackLimit), hi = reinterpret_cast<uintptr_t>(tib->StackBase);
    t.Put("\r\nframe chain (ebp):");
    uintptr_t frame = c->Ebp;
    for (int i = 0; i < 24 && frame >= lo && frame + 8 <= hi && !(frame & 3); ++i)
    {
        uintptr_t next = 0, ret = 0;
        __try
        {
            next = reinterpret_cast<const uintptr_t*>(frame)[0];
            ret = reinterpret_cast<const uintptr_t*>(frame)[1];
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            break;
        }
        t.Put("\r\n  ");
        Where(t, ret);
        if (next <= frame)
            break;
        frame = next;
    }
    // Code built without frame pointers breaks the chain: every value on the stack that returns behind a call, from esp up.
    t.Put("\r\nreturn addresses on the stack (esp up):");
    int found = 0;
    const uintptr_t from = c->Esp >= lo && c->Esp < hi ? (c->Esp & ~uintptr_t(3)) : 0;
    for (uintptr_t p = from; from && p + 4 <= hi && p < from + 0x4000 && found < 32; p += 4)
    {
        uintptr_t v = 0;
        __try
        {
            v = *reinterpret_cast<const uintptr_t*>(p);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            break;
        }
        if (!IsReturnAddress(v))
            continue;
        ++found;
        t.Put("\r\n  [esp+").Hex(static_cast<uint32_t>(p - from), 4).Put("] ");
        Where(t, v);
    }
    t.Put(g_miniDump ? "\r\nminidump: " : "\r\n");
    WriteOut(t.buf, t.len); // first: the block survives whatever the dump does
}

static DWORD WINAPI DumpThread(void*)
{
    HANDLE f = CreateFileW(g_dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return 2;
    // MiniDumpNormal | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo: every thread's stack and registers,
    // the module list, the memory the stacks point at
    const BOOL ok = g_miniDump(GetCurrentProcess(), GetCurrentProcessId(), f, 0x0040 | 0x1000, &g_dumpInfo, nullptr, nullptr);
    CloseHandle(f);
    return ok ? 0 : 1;
}

// A test start's dump, on a thread of its own with a time limit: dbghelp suspends the other threads and may need a lock one
// of them holds; the filter never waits longer than this for it.
static void WriteDump(EXCEPTION_POINTERS* ep)
{
    g_dumpInfo = {GetCurrentThreadId(), ep, FALSE};
    const char* result = "not started";
    if (HANDLE th = CreateThread(nullptr, 0, DumpThread, nullptr, 0, nullptr))
    {
        DWORD code = 3;
        result = WaitForSingleObject(th, 15000) != WAIT_OBJECT_0 ? "not finished in 15 s"
                 : !GetExitCodeThread(th, &code) || code       ? "failed"
                                                                : "FLvibed_crash.dmp beside this file";
        CloseHandle(th);
    }
    char line[96];
    Text t(line, sizeof(line));
    t.Put(result).Put("\r\n");
    WriteOut(t.buf, t.len);
}

static LONG WINAPI Filter(EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    if (ep && ep->ContextRecord && !crashrules::PassedOn(code) && InterlockedExchange(&g_taken, 1) == 0)
    {
        __try
        {
            Record(ep, "unhandled exception");
            if (g_miniDump)
                WriteDump(ep);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return g_prev ? g_prev(ep) : EXCEPTION_CONTINUE_SEARCH;
}

// The vectored handler: every exception's first chance passes through it, before any frame's handler. It records only the
// fatal codes (crashrules::FatalFirstChance) and always passes the exception on: what happens next is the game's and Windows'
// as before. An unwind to a frame not on the stack ends the game with no frame handler and no filter run (the filter above
// never sees it); here its block is written all the same. The first block only, as the filter's (g_taken): a fatal code
// recorded here is not recorded again there.
static LONG CALLBACK Vectored(EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0;
    if (ep && ep->ContextRecord && crashrules::FatalFirstChance(code) && InterlockedExchange(&g_taken, 1) == 0)
    {
        __try
        {
            Record(ep, "fatal exception at its first chance (vectored)");
            if (g_miniDump)
                WriteDump(ep);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// What a filter address is, for the log (outside a crash: the ordinary helpers are fine here).
static std::string Describe(const void* fn)
{
    if (!fn)
        return "none (Windows' own)";
    char b[160];
    Text t(b, sizeof(b));
    Where(t, reinterpret_cast<uintptr_t>(fn));
    return b;
}

#ifndef FLV_STATIC
// Test aid, dev builds only: FLVIBED_CRASH_TEST=thread (a thread of the mod's writes to address 0x10 at the first frame),
// =frame (the game's main thread does, inside the frame) or =unwind (a thread with no frame handler at all raises the invalid
// unwind target: no filter runs, as on a broken handler chain; only the vectored handler can write the block): the recorder's
// block for a known fault. Never in the release.
static DWORD WINAPI CrashTestThread(void*)
{
    *reinterpret_cast<volatile int*>(0x10) = 1;
    return 0;
}
static DWORD WINAPI CrashUnwindThread(void*)
{
    __writefsdword(0, 0xFFFFFFFFu); // (the thread's handler chain emptied: its start's frame, which calls the filter, gone)
    RaiseException(0xC0000029, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    return 0;
}

static void CrashTest()
{
    const std::string what = EnvVar("FLVIBED_CRASH_TEST");
    if (what.empty())
        return;
    Log("crash recorder: FLVIBED_CRASH_TEST=%s - a deliberate fault now (dev build test aid)", what.c_str());
    if (what == "thread")
    {
        if (HANDLE th = CreateThread(nullptr, 0, CrashTestThread, nullptr, 0, nullptr))
            CloseHandle(th);
    }
    else if (what == "frame")
        *reinterpret_cast<volatile int*>(0x10) = 1;
    else if (what == "unwind")
    {
        if (HANDLE th = CreateThread(nullptr, 0, CrashUnwindThread, nullptr, 0, nullptr))
            CloseHandle(th);
    }
}
#endif

void Install()
{
    if (g_installed)
        return;
    g_installed = true;
    g_mainThread = GetCurrentThreadId();
    g_mappedName = reinterpret_cast<MappedNameFn>(GetProcAddress(GetModuleHandleA("kernel32.dll"), "K32GetMappedFileNameW"));
    g_threadInfo = reinterpret_cast<ThreadInfoFn>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread"));
    // beside FLvibed.dll (the game's EXE folder, where its other logs are), in UTF-16: any install path
    MEMORY_BASIC_INFORMATION self = {};
    VirtualQuery(reinterpret_cast<const void*>(&Filter), &self, sizeof(self));
    g_selfBase = reinterpret_cast<uintptr_t>(self.AllocationBase);
    wchar_t dir[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(static_cast<HMODULE>(self.AllocationBase), dir, MAX_PATH);
    wchar_t* slash = n && n < MAX_PATH ? wcsrchr(dir, L'\\') : nullptr;
    if (!slash || (slash - dir) + 24 >= MAX_PATH)
    {
        Log("crash recorder: off (no path for its file)");
        return;
    }
    slash[1] = L'\0';
    wcscpy_s(g_logPath, dir);
    wcscat_s(g_logPath, L"FLvibed_crash.log");
    wcscpy_s(g_dumpPath, dir);
    wcscat_s(g_dumpPath, L"FLvibed_crash.dmp");
    // the dump only on a test start: dbghelp loaded here, never inside a crash (the loader lock may be held then)
    if (TestSwitch())
    {
        wchar_t sys[MAX_PATH];
        const UINT len = GetSystemDirectoryW(sys, MAX_PATH);
        if (len && len + 14 < MAX_PATH)
        {
            wcscat_s(sys, L"\\dbghelp.dll");
            if (HMODULE dbghelp = LoadLibraryW(sys))
                g_miniDump = reinterpret_cast<MiniDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
        }
    }
    g_prev = SetUnhandledExceptionFilter(Filter);
    const bool vectored = AddVectoredExceptionHandler(1, Vectored) != nullptr; // (for the life of the process, as the filter)
    Log("crash recorder: on (unhandled exceptions to FLvibed_crash.log%s%s); in front of %s", g_miniDump ? " + a minidump" : "",
        // (the word "fatal" stays out of this line: the tests read any line with it, in any case, as a fault)
        vectored ? ", the process-ending codes at their first chance" : "", Describe(reinterpret_cast<const void*>(g_prev)).c_str());
}

void FirstFrame()
{
    if (!g_installed || !g_logPath[0])
        return;
    const LPTOP_LEVEL_EXCEPTION_FILTER now = SetUnhandledExceptionFilter(Filter);
    if (now != Filter)
    {
        // the game's own filter (it never calls a previous one): set after the start, so the recorder goes in front of it
        // again; anything else is put back as it was - it may call the recorder, and a loop of filters must not start
        MEMORY_BASIC_INFORMATION m = {};
        const bool game = VirtualQuery(reinterpret_cast<const void*>(now), &m, sizeof(m)) &&
                          m.AllocationBase == reinterpret_cast<void*>(GetModuleHandleA(nullptr));
        if (game)
            g_prev = now;
        else
            SetUnhandledExceptionFilter(now);
        Log("crash recorder: the filter was replaced after the start by %s: %s", Describe(reinterpret_cast<const void*>(now)).c_str(),
            game ? "the recorder is in front of it again" : "left in place (the recorder runs only if it calls the one before it)");
    }
#ifndef FLV_STATIC
    CrashTest();
#endif
}
} // namespace crashlog
