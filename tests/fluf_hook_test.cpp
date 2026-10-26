// FLvibed's detours beside a restore-style detour (FLUF / FLSDK FunctionDetour), simulated: FlufStyleDetour below is
// FLSDK's algorithm (include\Utils\Detour.hpp, ae8c857): Detour() saves the 5 bytes at the function and writes its JMP;
// CallOriginalFunc() puts the saved bytes back, calls the function, then Detour() again (re-saving what is there).
// The "game functions" are machine code in FLvibed's executable pool; the FLUF handlers are compiled C++ (their start
// is what hk::Detour must move), marked as restore-style code for the test. Every order must run both handlers once per
// call, with the game's caller as FLvibed's return address once the hooks sit at FLUF's handlers.
#include "hook.h"

#include <intrin.h>

#include "test.h"

namespace
{
typedef int(__cdecl* GameFn)(int);

class FlufStyleDetour
{
  public:
    void Install(GameFn fn, GameFn handler)
    {
        orig_ = fn;
        handler_ = handler;
        DWORD old;
        VirtualProtect(reinterpret_cast<void*>(fn), 5, PAGE_EXECUTE_READWRITE, &old); // FLSDK's MemProtect
        Detour();
    }
    int CallOriginal(int x)
    {
        UnDetour();
        const int r = orig_(x);
        Detour();
        return r;
    }

  private:
    void Detour()
    {
        if (detoured_)
            return;
        detoured_ = true;
        u8 patch[5] = {0xE9};
        const u32 rel = static_cast<u32>(reinterpret_cast<uintptr_t>(handler_) - reinterpret_cast<uintptr_t>(orig_) - 5);
        memcpy(patch + 1, &rel, 4);
        memcpy(data_, reinterpret_cast<void*>(orig_), 5);
        memcpy(reinterpret_cast<void*>(orig_), patch, 5);
    }
    void UnDetour()
    {
        if (!detoured_)
            return;
        detoured_ = false;
        memcpy(reinterpret_cast<void*>(orig_), data_, 5);
    }
    GameFn orig_ = nullptr, handler_ = nullptr;
    u8 data_[5] = {};
    bool detoured_ = false;
};

struct Fluf
{
    FlufStyleDetour det;
    int hits = 0;
};
struct Vibed
{
    GameFn orig = nullptr;
    int hits = 0;
    void* ret = nullptr;
};
Fluf g_fluf[8];
Vibed g_vib[8];
volatile int g_sink = 0;

template <int N> int __cdecl FlufHandler(int x)
{
    ++g_fluf[N].hits;
    return g_fluf[N].det.CallOriginal(x);
}

template <int N> int __cdecl VibedHandler(int x)
{
    ++g_vib[N].hits;
    g_vib[N].ret = _ReturnAddress();
    return g_vib[N].orig(x);
}

// The game's caller (not a tail call: the return address lands in here).
__declspec(noinline) int CallSite(GameFn f, int x)
{
    const int r = f(x);
    g_sink = g_sink + r;
    return r;
}

// A function's code behind incremental-link thunks.
uintptr_t Body(const void* p)
{
    auto a = reinterpret_cast<uintptr_t>(p);
    for (int i = 0; i < 4 && *reinterpret_cast<const u8*>(a) == 0xE9; ++i)
        a = a + 5 + *reinterpret_cast<const i32*>(a + 1);
    return a;
}

bool FromCallSite(const void* ret)
{
    const uintptr_t b = Body(reinterpret_cast<const void*>(&CallSite)), r = reinterpret_cast<uintptr_t>(ret);
    return r > b && r < b + 64;
}

// cdecl int f(int x) { return x + add; }: push ebp; mov ebp,esp; mov eax,[ebp+8]; add eax,add; pop ebp; ret
// (FLvibed steals 6 bytes: the first three instructions)
GameFn MakeGame(u8 add)
{
    const u8 code[] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x83, 0xC0, add, 0x5D, 0xC3};
    void* p = hk::AllocExec(sizeof(code));
    memcpy(p, code, sizeof(code));
    return reinterpret_cast<GameFn>(p);
}

void ResetCounts()
{
    for (auto& f : g_fluf)
        f.hits = 0;
    for (auto& v : g_vib)
        v.hits = 0, v.ret = nullptr;
}

void TestDecoder()
{
    struct Case
    {
        u8 bytes[8];
        size_t len;
    } cases[] = {
        {{0x55}, 1},
        {{0x8B, 0xEC}, 2},
        {{0x83, 0xEC, 0x10}, 3},
        {{0x81, 0xEC, 0x00, 0x01, 0x00, 0x00}, 6},
        {{0x6A, 0xFF}, 2},
        {{0x68, 1, 2, 3, 4}, 5},
        {{0x64, 0xA1, 0, 0, 0, 0}, 6},
        {{0xA1, 1, 2, 3, 4}, 5},
        {{0x8B, 0x0D, 1, 2, 3, 4}, 6},
        {{0xFF, 0x05, 1, 2, 3, 4}, 6},
        {{0xF2, 0x0F, 0x10, 0x44, 0x24, 0x04}, 6},
        {{0x66, 0x0F, 0xD6, 0x45, 0xF0}, 5},
        {{0x8B, 0x44, 0x24, 0x04}, 4},
        {{0x8B, 0x84, 0x24, 0, 1, 0, 0}, 7},
        {{0xC7, 0x45, 0xFC, 0, 0, 0, 0}, 7},
        {{0x66, 0xC7, 0x45, 0xFC, 1, 0}, 6},
        {{0x83, 0x3D, 1, 2, 3, 4, 0}, 7},
        {{0x80, 0x3D, 1, 2, 3, 4, 0}, 7},
        {{0xE8, 1, 2, 3, 4}, 5},
        {{0x74, 0x05}, 0},
        {{0x0F, 0x84, 1, 2, 3, 4}, 0},
        {{0xCC}, 0},
        {{0xE2, 0xFE}, 0},
        {{0x67, 0x8B, 0x00}, 0},
    };
    for (const Case& c : cases)
    {
        const size_t n = hk::MovableInsnLength(c.bytes);
        CHECK(n == c.len);
        if (n != c.len)
            fprintf(stderr, "  decoder: %02X %02X -> %u, want %u\n", c.bytes[0], c.bytes[1], static_cast<u32>(n),
                    static_cast<u32>(c.len));
    }
    bool term = false;
    const u8 ret[] = {0xC3};
    CHECK(hk::MovableInsnLength(ret, &term) == 1 && term);
}
} // namespace

void Test_fluf_hooks()
{
    TestDecoder();

    // The test exe's code counts as FLUF (its handlers live there); FLvibed's own stubs are in its pool.
    const auto exe = reinterpret_cast<const u8*>(GetModuleHandleA(nullptr));
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(exe + reinterpret_cast<const IMAGE_DOS_HEADER*>(exe)->e_lfanew);
    hk::AddRestoreStyleRange(reinterpret_cast<uintptr_t>(exe), reinterpret_cast<uintptr_t>(exe) + nt->OptionalHeader.SizeOfImage);

    // 1. FLvibed first, then FLUF: FLUF saves FLvibed's JMP as "the original"; both run, FLvibed inside (its caller is
    //    FLUF). AdoptRestoreStyle moves the hook to FLUF's handler: both run, FLvibed sees the game's caller.
    {
        ResetCounts();
        const GameFn game = MakeGame(1);
        g_vib[0].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<0>, 6, "t1"));
        CHECK(g_vib[0].orig != nullptr);
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookDirect);
        g_fluf[0].det.Install(game, FlufHandler<0>);
        for (int i = 0; i < 2; ++i)
            CHECK(CallSite(game, 5) == 6);
        CHECK(g_vib[0].hits == 2 && g_fluf[0].hits == 2);
        CHECK(!FromCallSite(g_vib[0].ret)); // inside FLUF's CallOriginalFunc: the caller-based checks would fail
        CHECK(hk::AdoptRestoreStyle("test") == 1);
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookAtHandler);
        for (int i = 0; i < 3; ++i)
            CHECK(CallSite(game, 10) == 11);
        CHECK(g_vib[0].hits == 5 && g_fluf[0].hits == 5);
        CHECK(FromCallSite(g_vib[0].ret));
        CHECK(hk::AdoptRestoreStyle("test") == 0); // once moved, stays
        CHECK(CallSite(game, 1) == 2 && g_vib[0].hits == 6 && g_fluf[0].hits == 6);
    }

    // 2. FLUF first, then FLvibed: hooked at FLUF's handler at once (a JMP over FLUF's would be erased by its first
    //    call: that was the silent loss).
    {
        ResetCounts();
        const GameFn game = MakeGame(2);
        g_fluf[1].det.Install(game, FlufHandler<1>);
        g_vib[1].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<1>, 6, "t2"));
        CHECK(g_vib[1].orig != nullptr);
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookAtHandler);
        for (int i = 0; i < 4; ++i)
            CHECK(CallSite(game, 5) == 7);
        CHECK(g_vib[1].hits == 4 && g_fluf[1].hits == 4);
        CHECK(FromCallSite(g_vib[1].ret));
    }

    // 3. Two FLvibed parts (subtitles, controller), then FLUF, then both adopt.
    {
        ResetCounts();
        const GameFn game = MakeGame(3);
        g_vib[2].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<2>, 6, "t3a"));
        g_vib[3].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<3>, 6, "t3b"));
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookChained);
        g_fluf[2].det.Install(game, FlufHandler<2>);
        CHECK(CallSite(game, 5) == 8);
        CHECK(g_vib[2].hits == 1 && g_vib[3].hits == 1 && g_fluf[2].hits == 1);
        CHECK(hk::AdoptRestoreStyle("test") == 2);
        for (int i = 0; i < 3; ++i)
            CHECK(CallSite(game, 5) == 8);
        CHECK(g_vib[2].hits == 4 && g_vib[3].hits == 4 && g_fluf[2].hits == 4);
        CHECK(FromCallSite(g_vib[2].ret) && FromCallSite(g_vib[3].ret));
    }

    // 4. FLUF first, then two FLvibed parts.
    {
        ResetCounts();
        const GameFn game = MakeGame(4);
        g_fluf[3].det.Install(game, FlufHandler<3>);
        g_vib[4].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<4>, 6, "t4a"));
        g_vib[5].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<5>, 6, "t4b"));
        CHECK(g_vib[4].orig && g_vib[5].orig);
        for (int i = 0; i < 3; ++i)
            CHECK(CallSite(game, 5) == 9);
        CHECK(g_vib[4].hits == 3 && g_vib[5].hits == 3 && g_fluf[3].hits == 3);
        CHECK(FromCallSite(g_vib[4].ret) && FromCallSite(g_vib[5].ret));
        CHECK(hk::AdoptRestoreStyle("test") == 0);
    }

    // 5. Fails closed: FLUF first with a handler whose start cannot be moved (a conditional jump) -> refused, nothing
    //    written.
    {
        const GameFn game = MakeGame(5);
        u8* fake = static_cast<u8*>(hk::AllocExec(16));
        const u8 code[] = {0x74, 0x00, 0x31, 0xC0, 0xC3, 0x90, 0x90, 0x90}; // jz +0; xor eax,eax; ret
        memcpy(fake, code, sizeof(code));
        hk::AddRestoreStyleRange(reinterpret_cast<uintptr_t>(fake), reinterpret_cast<uintptr_t>(fake) + 16);
        g_fluf[4].det.Install(game, reinterpret_cast<GameFn>(fake));
        u8 before[8], after[8];
        memcpy(before, reinterpret_cast<void*>(game), 8);
        const u8 fakeBefore = fake[0];
        CHECK(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<6>, 6, "t5") == nullptr);
        memcpy(after, reinterpret_cast<void*>(game), 8);
        CHECK(memcmp(before, after, 8) == 0 && fake[0] == fakeBefore);
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookNone);
    }

    // 7. FLUF 2.0.0's HandleKey hook: `mov ecx, handler; jmp ecx` written over the start (no saved bytes); its naked
    //    handler counts, replays the overwritten instructions and jumps back. Either order: FLvibed hooks the handler.
    {
        static volatile LONG overwriteHits[2] = {};
        auto makeOverwrite = [](GameFn game, int slot, u8 add) {
            u8* h = static_cast<u8*>(hk::AllocExec(32));
            size_t n = 0;
            h[n++] = 0xFF; // inc dword ptr [counter]
            h[n++] = 0x05;
            *reinterpret_cast<u32*>(h + n) = static_cast<u32>(reinterpret_cast<uintptr_t>(&overwriteHits[slot]));
            n += 4;
            const u8 replay[9] = {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08, 0x83, 0xC0, add}; // the original start, as FLUF hard-codes it
            memcpy(h + n, replay, 9);
            n += 9;
            h[n] = 0xE9;
            *reinterpret_cast<i32*>(h + n + 1) =
                static_cast<i32>(reinterpret_cast<uintptr_t>(game) + 9 - (reinterpret_cast<uintptr_t>(h + n) + 5));
            hk::AddRestoreStyleRange(reinterpret_cast<uintptr_t>(h), reinterpret_cast<uintptr_t>(h) + 32);
            u8 patch[9] = {0xB9, 0, 0, 0, 0, 0xFF, 0xE1, 0x90, 0x90};
            *reinterpret_cast<u32*>(patch + 1) = static_cast<u32>(reinterpret_cast<uintptr_t>(h));
            DWORD old;
            VirtualProtect(reinterpret_cast<void*>(game), 9, PAGE_EXECUTE_READWRITE, &old);
            memcpy(reinterpret_cast<void*>(game), patch, 9);
        };
        // FLUF first
        ResetCounts();
        const GameFn g1 = MakeGame(7);
        makeOverwrite(g1, 0, 7);
        g_vib[0].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(g1), VibedHandler<0>, 6, "t7a"));
        CHECK(g_vib[0].orig != nullptr);
        CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(g1)) == hk::HookAtHandler);
        for (int i = 0; i < 3; ++i)
            CHECK(CallSite(g1, 5) == 12);
        CHECK(g_vib[0].hits == 3 && overwriteHits[0] == 3);
        CHECK(FromCallSite(g_vib[0].ret));
        // FLvibed first: the overwrite erases FLvibed's jump; the first frame's adoption hooks the handler
        ResetCounts();
        const GameFn g2 = MakeGame(8);
        g_vib[1].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(g2), VibedHandler<1>, 6, "t7b"));
        makeOverwrite(g2, 1, 8);
        CHECK(CallSite(g2, 5) == 13);
        CHECK(g_vib[1].hits == 0 && overwriteHits[1] == 1); // lost until the adoption
        CHECK(hk::AdoptRestoreStyle("test") == 1);
        for (int i = 0; i < 2; ++i)
            CHECK(CallSite(g2, 5) == 13);
        CHECK(g_vib[1].hits == 2 && overwriteHits[1] == 3);
        CHECK(FromCallSite(g_vib[1].ret));
    }

    // 8. FLUF nightly-2026-10-16-03-03 (627ea2c): the entry bytes of the six handlers FLvibed hooks, as the shipped
    //    FLUF.dll has them (research\evidence\fluf_nightly\handler_entry_bytes.txt; RVAs from its PDB). Each start must
    //    move whole (no relative branch, no early return) and a hook over a JMP into such a handler must install at the
    //    handler with exactly those instructions moved (the code is installed, not run: the imm32s are FLUF's).
    {
        struct Handler
        {
            const char* name;
            u8 bytes[12];
            size_t moved; // the whole instructions covering 5 bytes
        } handlers[] = {
            {"OnUpdate (FrameUpdate)", {0x55, 0x53, 0x57, 0x56, 0x83, 0xEC, 0x0C, 0xF2, 0x0F, 0x10, 0x05, 0x48}, 7},
            {"UiRenderDetour", {0x53, 0x57, 0x56, 0x8B, 0x35, 0x20, 0xB8, 0x10, 0x11, 0x8B, 0x4E, 0x68}, 9},
            {"CursorRenderDetour", {0x53, 0x56, 0x8B, 0x35, 0x20, 0xB8, 0x10, 0x11, 0x8B, 0x4E, 0x68, 0x85}, 8},
            {"WinKeyDetour (outer)", {0x55, 0x53, 0x57, 0x56, 0x8B, 0x6C, 0x24, 0x1C, 0x8B, 0x5C, 0x24, 0x18}, 8},
            {"HandleKeyDetour", {0x53, 0x57, 0x56, 0x83, 0xEC, 0x08, 0xA1, 0x80, 0x26, 0xF2, 0x10, 0x31}, 6},
            {"CreateSoundDetour (PlayArch)", {0x55, 0x53, 0x57, 0x56, 0x83, 0xEC, 0x10, 0x8B, 0x7C, 0x24, 0x2C, 0x8B}, 7},
        };
        for (const Handler& h : handlers)
        {
            size_t n = 0;
            bool term = false, ok = true;
            while (n < 5 && ok)
            {
                const size_t len = hk::MovableInsnLength(h.bytes + n, &term);
                ok = len && !term;
                n += len;
            }
            CHECK(ok && n == h.moved);
            if (!ok || n != h.moved)
                fprintf(stderr, "  nightly handler %s: moved %u, want %u\n", h.name, static_cast<u32>(n),
                        static_cast<u32>(h.moved));
            // FLUF first: the game function's JMP leads into the handler (restore-style code)
            u8* handler = static_cast<u8*>(hk::AllocExec(32));
            memset(handler, 0xCC, 32);
            memcpy(handler, h.bytes, sizeof(h.bytes));
            hk::AddRestoreStyleRange(reinterpret_cast<uintptr_t>(handler), reinterpret_cast<uintptr_t>(handler) + 32);
            const GameFn game = MakeGame(9);
            u8 jmp[5] = {0xE9};
            *reinterpret_cast<i32*>(jmp + 1) =
                static_cast<i32>(reinterpret_cast<uintptr_t>(handler) - (reinterpret_cast<uintptr_t>(game) + 5));
            DWORD old;
            VirtualProtect(reinterpret_cast<void*>(game), 5, PAGE_EXECUTE_READWRITE, &old);
            memcpy(reinterpret_cast<void*>(game), jmp, 5);
            void* orig = hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<7>, 6, h.name);
            CHECK(orig != nullptr);
            CHECK(hk::ModeOf(reinterpret_cast<uintptr_t>(game)) == hk::HookAtHandler);
            CHECK(memcmp(reinterpret_cast<void*>(game), jmp, 5) == 0); // FLUF's jump untouched (it re-writes it anyway)
            CHECK(handler[0] == 0xE9);                                  // FLvibed's jump at the handler ...
            bool padded = true;
            for (size_t i = 5; i < h.moved; ++i)
                padded = padded && handler[i] == 0x90;
            CHECK(padded && memcmp(handler + h.moved, h.bytes + h.moved, sizeof(h.bytes) - h.moved) == 0); // ... the rest intact
        }
    }

    // 6. No FLUF: a plain hook and a plain chain still run once each (the behaviour on HD Edition).
    {
        hk::ClearRestoreStyleRanges();
        ResetCounts();
        const GameFn game = MakeGame(6);
        g_vib[6].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<6>, 6, "t6a"));
        g_vib[7].orig = static_cast<GameFn>(hk::Detour(reinterpret_cast<uintptr_t>(game), VibedHandler<7>, 6, "t6b"));
        CHECK(CallSite(game, 5) == 11);
        CHECK(g_vib[6].hits == 1 && g_vib[7].hits == 1);
        CHECK(FromCallSite(g_vib[6].ret) && FromCallSite(g_vib[7].ret));
        CHECK(hk::AdoptRestoreStyle("test") == 0);
    }
    hk::ClearRestoreStyleRanges();
}
