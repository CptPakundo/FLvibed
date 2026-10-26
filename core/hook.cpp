#include "hook.h"

#include "log.h"

namespace hk
{
// ------------------------------------------------------------------ symbols
static Image g_image = {};
// Test switch: skip the v1.1 fast path so every symbol is found by its signature (build with
// `set CL=/DFLV_SIG_SCAN_ONLY` before build.cmd; sigtool check --scan).
#ifdef FLV_SIG_SCAN_ONLY
static bool g_scanOnly = true;
#else
static bool g_scanOnly = false;
#endif
void SetScanOnly(bool on) { g_scanOnly = on; }

static Image ProcessImage()
{
    Image img = {};
    const u8* base = reinterpret_cast<const u8*>(GetModuleHandleA(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    img.mem = base;
    img.base = static_cast<u32>(reinterpret_cast<uintptr_t>(base));
    img.size = nt->OptionalHeader.SizeOfImage;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
    {
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)
        {
            img.textVa = img.base + sec->VirtualAddress;
            img.textSize = sec->Misc.VirtualSize;
            break;
        }
    }
    return img;
}

void SetImage(const Image& image) { g_image = image; }

const Image& GetImage()
{
    if (!g_image.mem)
        g_image = ProcessImage();
    return g_image;
}

static bool InText(u32 va, size_t len)
{
    const Image& img = GetImage();
    return va >= img.textVa && va + len <= img.textVa + img.textSize;
}

static const u8* Ptr(u32 va) { return GetImage().mem + (va - GetImage().base); }

bool ParseSig(const char* sig, std::vector<u8>& bytes, std::vector<u8>& mask)
{
    bytes.clear();
    mask.clear();
    for (const char* p = sig; *p;)
    {
        if (*p == ' ')
        {
            ++p;
            continue;
        }
        if (p[0] == '?' && p[1] == '?')
        {
            bytes.push_back(0);
            mask.push_back(0);
            p += 2;
            continue;
        }
        char hex[3] = {p[0], p[1], 0};
        char* end = nullptr;
        const unsigned long v = strtoul(hex, &end, 16);
        if (end != hex + 2)
            return false;
        bytes.push_back(static_cast<u8>(v));
        mask.push_back(1);
        p += 2;
    }
    return !bytes.empty();
}

// The bytes a detour may overwrite at a code address: JMP + NOP padding (FLvibed's longest steals 8).
constexpr size_t kDetourSpan = 8;

// Another hooker's jump at a function's start: E9 rel32 (5 bytes) or `mov ecx, imm32; jmp ecx` (B9 imm32 FF E1, 7 bytes:
// FLUF 2.0.0's HandleKey). Its length, 0 = none; `to` = where it goes.
static size_t ForeignJumpLength(const u8* c, uintptr_t* to = nullptr)
{
    if (c[0] == 0xE9)
    {
        if (to)
            *to = reinterpret_cast<uintptr_t>(c) + 5 + *reinterpret_cast<const i32*>(c + 1);
        return 5;
    }
    if (c[0] == 0xB9 && c[5] == 0xFF && c[6] == 0xE1)
    {
        if (to)
            *to = *reinterpret_cast<const u32*>(c + 1);
        return 7;
    }
    return 0;
}

bool MatchAt(const u8* pattern, const u8* mask, size_t len, u32 at, int hookAt)
{
    if (!InText(at, len))
        return false;
    const u8* code = Ptr(at);
    // A JMP at the code address: someone's detour. Skip its 5 bytes and the NOPs padding its stolen bytes, at most
    // kDetourSpan in all (a JMP followed by alignment padding must not swallow the signature; sigtool keeps every
    // code signature unique without these bytes).
    // FLUF's HandleKey hook (2.0.0) is `mov ecx, imm32; jmp ecx` (7 bytes) instead of a JMP: skipped the same way.
    size_t skipFrom = len, skipTo = len;
    const size_t jumpLen = hookAt >= 0 ? ForeignJumpLength(code + hookAt) : 0;
    if (jumpLen && static_cast<size_t>(hookAt) + jumpLen <= len)
    {
        skipFrom = hookAt;
        skipTo = hookAt + jumpLen;
        while (skipTo < len && skipTo < static_cast<size_t>(hookAt) + kDetourSpan && code[skipTo] == 0x90 &&
               pattern[skipTo] != 0x90)
            ++skipTo;
    }
    for (size_t i = 0; i < len; ++i)
    {
        if (i >= skipFrom && i < skipTo)
            continue;
        if (mask[i] && code[i] != pattern[i])
            return false;
    }
    return true;
}

const char* StateName(SymState state)
{
    switch (state)
    {
    case SymV11: return "v1.1";
    case SymScanned: return "found by signature";
    case SymMissing: return "signature mismatch";
    case SymAmbiguous: return "signature ambiguous";
    default: return "unresolved";
    }
}

// Search the signature in .text: the number of matches (stops at 2) and the first one's start VA. The anchor (the
// byte searched with memchr) is the last literal byte, past any detour JMP at the code address.
static int Scan(const std::vector<u8>& bytes, const std::vector<u8>& mask, int hookAt, u32& first)
{
    const Image& img = GetImage();
    size_t anchor = bytes.size();
    while (anchor > 0 && !mask[anchor - 1])
        --anchor;
    if (!anchor)
        return 0;
    --anchor;
    const u8* text = Ptr(img.textVa);
    int count = 0;
    for (u32 i = static_cast<u32>(anchor); i < img.textSize;)
    {
        const void* hit = memchr(text + i, bytes[anchor], img.textSize - i);
        if (!hit)
            break;
        const u32 pos = static_cast<u32>(static_cast<const u8*>(hit) - text);
        const u32 at = img.textVa + pos - static_cast<u32>(anchor);
        if (MatchAt(bytes.data(), mask.data(), bytes.size(), at, hookAt))
        {
            if (!count)
                first = at;
            else
                Log("Freelancer.exe: signature matches at 0x%06X and 0x%06X", first, at);
            if (++count > 1)
                break;
        }
        i = pos + 1;
    }
    return count;
}

bool Resolve(Sym& s)
{
    if (s.state != SymUnresolved)
        return s.addr != 0;
    std::vector<u8> bytes, mask;
    if (!ParseSig(s.sig, bytes, mask) || s.off < 0 || static_cast<size_t>(s.off) >= bytes.size())
    {
        s.state = SymMissing;
        return false;
    }
    const int hookAt = s.kind == Code ? s.off : -1;
    // Fast path: the known v1.1 location, its bytes verified.
    const u32 at = (s.kind == Code ? s.va : s.site) - s.off;
    if (!g_scanOnly && MatchAt(bytes.data(), mask.data(), bytes.size(), at, hookAt) &&
        (s.kind == Code || *reinterpret_cast<const u32*>(Ptr(s.site)) == s.va))
    {
        s.addr = s.va;
        s.state = SymV11;
        return true;
    }
    // Another build: the signature must match exactly once in .text.
    u32 match = 0;
    const int count = Scan(bytes, mask, hookAt, match);
    if (count != 1)
    {
        s.state = count ? SymAmbiguous : SymMissing;
        return false;
    }
    const Image& img = GetImage();
    const u32 addr = s.kind == Code ? match + s.off : *reinterpret_cast<const u32*>(Ptr(match + s.off));
    if (addr < img.base || addr >= img.base + img.size) // a data operand must point into the image
    {
        s.state = SymMissing;
        return false;
    }
    s.addr = addr;
    s.state = SymScanned;
    return true;
}

uintptr_t Sym::get()
{
    if (state == SymUnresolved && !Resolve(*this))
        Log("Freelancer.exe: %s (v1.1 0x%06X) %s: calls and reads refused", name, va, StateName(state));
    return addr;
}

void ResolveAll(Sym* const* syms, size_t count, const char* who)
{
    int counts[5] = {};
    LARGE_INTEGER t0, t1, freq;
    QueryPerformanceCounter(&t0);
    for (size_t i = 0; i < count; ++i)
    {
        Sym& s = *syms[i];
        Resolve(s);
        ++counts[s.state];
        if (s.state != SymV11)
            Log("%s: Freelancer.exe %s (v1.1 0x%06X): %s%s", who, s.name, s.va, StateName(s.state),
                s.addr ? "" : " - refused");
        if (s.state == SymScanned)
            Log("%s:   %s at 0x%06X%s", who, s.name, s.addr, s.addr == s.va ? " (= v1.1)" : "");
    }
    QueryPerformanceCounter(&t1);
    QueryPerformanceFrequency(&freq);
    Log("%s: Freelancer.exe symbols: %d v1.1, %d found by signature, %d unresolved (%.1f ms)", who, counts[SymV11],
        counts[SymScanned], counts[SymMissing] + counts[SymAmbiguous],
        1000.0 * (t1.QuadPart - t0.QuadPart) / freq.QuadPart);
}

// ------------------------------------------------------------------ patching
bool Write(uintptr_t addr, const void* data, size_t len)
{
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(addr), len, PAGE_EXECUTE_READWRITE, &old))
        return false;
    memcpy(reinterpret_cast<void*>(addr), data, len);
    VirtualProtect(reinterpret_cast<void*>(addr), len, old, &old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(addr), len);
    return true;
}

static u8* g_pool = nullptr;
static size_t g_poolUsed = 0;
static const size_t kPoolSize = 0x10000;

void* AllocExec(size_t len)
{
    if (!g_pool)
    {
        g_pool = static_cast<u8*>(VirtualAlloc(nullptr, kPoolSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!g_pool)
            return nullptr;
    }
    len = (len + 15) & ~size_t(15);
    if (g_poolUsed + len > kPoolSize)
        return nullptr;
    void* p = g_pool + g_poolUsed;
    g_poolUsed += len;
    return p;
}

static void WriteJmp(u8* at, uintptr_t to)
{
    at[0] = 0xE9;
    *reinterpret_cast<i32*>(at + 1) = static_cast<i32>(to - (reinterpret_cast<uintptr_t>(at) + 5));
}

// ------------------------------------------------------------------ instruction lengths (moving a foreign prologue)
// x86-32 only, the instructions compilers put at a function's start. Refused (0): relative conditional jumps / loops
// (their target would need a rewrite FLvibed does not do), far transfers, int3 (padding: the function is shorter than
// the patch), the 0x67 address-size prefix, unknown 0F opcodes.
static size_t ModRmLen(const u8* m)
{
    const u8 mod = m[0] >> 6, rm = m[0] & 7;
    size_t n = 1;
    if (mod == 3)
        return n;
    if (rm == 4)
    {
        ++n;
        if (mod == 0 && (m[1] & 7) == 5)
            n += 4;
    }
    else if (mod == 0 && rm == 5)
        n += 4;
    if (mod == 1)
        n += 1;
    else if (mod == 2)
        n += 4;
    return n;
}

enum InsnRel : u8
{
    RelNone,
    Rel32, // E8 / E9 rel32 (no prefix)
    Rel8,  // EB rel8 (no prefix)
};

static size_t DecodeInsn(const u8* code, InsnRel& rel, bool& terminal)
{
    rel = RelNone;
    terminal = false;
    size_t i = 0;
    bool op16 = false;
    for (;; ++i)
    {
        if (i > 4)
            return 0;
        const u8 b = code[i];
        if (b == 0x66)
            op16 = true;
        else if (b == 0x67)
            return 0;
        else if (b != 0xF0 && b != 0xF2 && b != 0xF3 && b != 0x2E && b != 0x36 && b != 0x3E && b != 0x26 &&
                 b != 0x64 && b != 0x65)
            break;
    }
    const bool prefixed = i != 0;
    const u8 op = code[i++];
    const size_t iz = op16 ? 2 : 4;
    size_t n = 0; // bytes after the opcode
    if (op == 0x0F)
    {
        const u8 op2 = code[i++];
        if (op2 >= 0x80 && op2 <= 0x8F)
            return 0; // jcc rel32
        if (op2 == 0x38)
            n = 1 + ModRmLen(code + i + 1);
        else if (op2 == 0x3A)
            n = 1 + ModRmLen(code + i + 1) + 1;
        else if (op2 == 0x31 || op2 == 0xA2 || op2 == 0x77 || op2 == 0xA0 || op2 == 0xA1 || op2 == 0xA8 ||
                 op2 == 0xA9 || (op2 >= 0xC8 && op2 <= 0xCF))
            n = 0;
        else if (op2 <= 0x0F && op2 != 0x0D)
            return 0; // system instructions, ud2, 3DNow!
        else if ((op2 >= 0x70 && op2 <= 0x73) || op2 == 0xA4 || op2 == 0xAC || op2 == 0xBA || op2 == 0xC2 ||
                 op2 == 0xC4 || op2 == 0xC5 || op2 == 0xC6)
            n = ModRmLen(code + i) + 1;
        else if ((op2 >= 0x30 && op2 <= 0x37) || op2 == 0xAA)
            return 0;
        else
            n = ModRmLen(code + i);
        return i + n;
    }
    if (op < 0x40)
    {
        const u8 lo = op & 7;
        n = lo <= 3 ? ModRmLen(code + i) : lo == 4 ? 1 : lo == 5 ? iz : 0;
        return i + n;
    }
    if (op < 0x60)
        return i;
    switch (op)
    {
    case 0x60: case 0x61: case 0x6C: case 0x6D: case 0x6E: case 0x6F: n = 0; break;
    case 0x62: case 0x63: n = ModRmLen(code + i); break;
    case 0x68: n = iz; break;
    case 0x69: n = ModRmLen(code + i) + iz; break;
    case 0x6A: n = 1; break;
    case 0x6B: n = ModRmLen(code + i) + 1; break;
    case 0x80: case 0x82: case 0x83: n = ModRmLen(code + i) + 1; break;
    case 0x81: n = ModRmLen(code + i) + iz; break;
    case 0x9A: return 0;
    case 0xA0: case 0xA1: case 0xA2: case 0xA3: n = 4; break;
    case 0xA8: n = 1; break;
    case 0xA9: n = iz; break;
    case 0xC0: case 0xC1: case 0xC6: n = ModRmLen(code + i) + 1; break;
    case 0xC2: n = 2; terminal = true; break;
    case 0xC3: terminal = true; break;
    case 0xC4: case 0xC5: n = ModRmLen(code + i); break;
    case 0xC7: n = ModRmLen(code + i) + iz; break;
    case 0xC8: n = 3; break;
    case 0xCA: n = 2; terminal = true; break;
    case 0xCB: case 0xCF: terminal = true; break;
    case 0xCC: return 0;
    case 0xCD: n = 1; break;
    case 0xD4: case 0xD5: n = 1; break;
    case 0xE0: case 0xE1: case 0xE2: case 0xE3: return 0;
    case 0xE4: case 0xE5: case 0xE6: case 0xE7: n = 1; break;
    case 0xE8:
    case 0xE9:
        if (prefixed)
            return 0;
        n = 4;
        rel = Rel32;
        terminal = op == 0xE9;
        break;
    case 0xEA: return 0;
    case 0xEB:
        if (prefixed)
            return 0;
        n = 1;
        rel = Rel8;
        terminal = true;
        break;
    case 0xF6: n = ModRmLen(code + i) + (((code[i] >> 3) & 7) < 2 ? 1 : 0); break;
    case 0xF7: n = ModRmLen(code + i) + (((code[i] >> 3) & 7) < 2 ? iz : 0); break;
    case 0xFE: n = ModRmLen(code + i); break;
    case 0xFF:
    {
        const u8 reg = (code[i] >> 3) & 7;
        if (reg == 3 || reg == 5 || reg == 7)
            return 0; // far call / jmp, invalid
        n = ModRmLen(code + i);
        terminal = reg == 4; // jmp r/m (absolute: movable)
        break;
    }
    default:
        if (op >= 0x70 && op <= 0x7F)
            return 0; // jcc rel8
        if ((op >= 0x84 && op <= 0x8F) || (op >= 0xD0 && op <= 0xD3) || (op >= 0xD8 && op <= 0xDF))
            n = ModRmLen(code + i);
        else if (op >= 0xB0 && op <= 0xB7)
            n = 1;
        else if (op >= 0xB8 && op <= 0xBF)
            n = iz;
        else
            n = 0; // 90-9F, A4-AF, C9, CE, D6, D7, EC-EF, F1, F4, F5, F8-FD
        break;
    }
    return i + n;
}

size_t MovableInsnLength(const u8* code, bool* terminal)
{
    InsnRel rel;
    bool term;
    const size_t n = DecodeInsn(code, rel, term);
    if (terminal)
        *terminal = term;
    return n;
}

static void WriteRel32(u8* at, u8 op, uintptr_t to)
{
    at[0] = op;
    *reinterpret_cast<i32*>(at + 1) = static_cast<i32>(to - (reinterpret_cast<uintptr_t>(at) + 5));
}

// Move the whole instructions that cover at least `min` bytes at `src` into `dst` (E8 / E9 rel32 and EB rel8
// relocated), then a JMP back after them. Returns the bytes taken at `src`, 0 when refused (`why` says why).
static size_t MovePrologue(const u8* src, size_t min, u8* dst, char* why, size_t whyLen)
{
    size_t s = 0, d = 0;
    bool term = false;
    while (s < min)
    {
        InsnRel rel;
        const size_t n = DecodeInsn(src + s, rel, term);
        if (!n)
        {
            _snprintf_s(why, whyLen, _TRUNCATE, "instruction %02X %02X at +%u cannot be moved", src[s], src[s + 1],
                        static_cast<u32>(s));
            return 0;
        }
        if (term && s + n < min)
        {
            _snprintf_s(why, whyLen, _TRUNCATE, "the function ends at +%u, before the %u-byte jump", static_cast<u32>(s + n),
                        static_cast<u32>(min));
            return 0;
        }
        if (rel == Rel32)
        {
            const uintptr_t to = reinterpret_cast<uintptr_t>(src + s + 5) + *reinterpret_cast<const i32*>(src + s + 1);
            WriteRel32(dst + d, src[s], to);
            d += 5;
        }
        else if (rel == Rel8)
        {
            const uintptr_t to = reinterpret_cast<uintptr_t>(src + s + 2) + static_cast<signed char>(src[s + 1]);
            WriteRel32(dst + d, 0xE9, to);
            d += 5;
        }
        else
        {
            memcpy(dst + d, src + s, n);
            d += n;
        }
        s += n;
    }
    if (!term)
        WriteRel32(dst + d, 0xE9, reinterpret_cast<uintptr_t>(src + s));
    return s;
}

// ------------------------------------------------------------------ restore-style detourers (FLUF)
static std::vector<std::pair<uintptr_t, uintptr_t>> g_rsRanges; // tests

void AddRestoreStyleRange(uintptr_t begin, uintptr_t end) { g_rsRanges.push_back({begin, end}); }
void ClearRestoreStyleRanges() { g_rsRanges.clear(); }

static bool ImportsDll(HMODULE m, const char* dll)
{
    __try
    {
        auto base = reinterpret_cast<const u8*>(m);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress)
            return false;
        for (auto imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp)
            if (_stricmp(reinterpret_cast<const char*>(base + imp->Name), dll) == 0)
                return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return false;
}

static HMODULE ModuleOf(uintptr_t addr)
{
    HMODULE m = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(addr), &m))
        return nullptr;
    return m;
}

static const char* BaseName(HMODULE m, char* buf, size_t len)
{
    if (!m || !GetModuleFileNameA(m, buf, static_cast<DWORD>(len)))
        return "?";
    const char* slash = strrchr(buf, '\\');
    return slash ? slash + 1 : buf;
}

// "FLUF.dll+0x1234" (or the raw address outside any module)
static std::string Where(uintptr_t addr)
{
    char buf[MAX_PATH], out[MAX_PATH + 24];
    HMODULE m = ModuleOf(addr);
    if (!m)
        _snprintf_s(out, sizeof(out), _TRUNCATE, "0x%08X", static_cast<u32>(addr));
    else
        _snprintf_s(out, sizeof(out), _TRUNCATE, "%s+0x%X", BaseName(m, buf, sizeof(buf)),
                    static_cast<u32>(addr - reinterpret_cast<uintptr_t>(m)));
    return out;
}

// ------------------------------------------------------------------ a module's original image (its own file)
// Another mod may rewrite a vtable slot, an instruction or a pointer in a game module at run time (FLUF: vt[0xA8] of
// every control class, Common.dll vtables, IServerImpl slots, the lane code). The module's file says what was there
// before anyone patched it: loaded as the Windows loader would (headers, sections at their RVAs, base relocations to
// the live base), once per module, only when a caller asks (no FLUF = nothing of this runs). Refused (null) when the
// file's TimeDateStamp / SizeOfImage are not the loaded image's, the file is unreadable, or a needed relocation form is
// unknown - the caller then fails closed as before.
struct OrigImage
{
    HMODULE m;
    u32 stamp, size;
    std::vector<u8> img; // empty = refused
};
static std::vector<OrigImage*> g_orig;
static SRWLOCK g_origLock = SRWLOCK_INIT;

static bool LoadOriginal(HMODULE m, const IMAGE_NT_HEADERS32* live, std::vector<u8>& img)
{
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(m, path, MAX_PATH))
        return false;
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER len = {};
    std::vector<u8> file;
    DWORD got = 0;
    const bool read = GetFileSizeEx(f, &len) && len.QuadPart > 0 && len.QuadPart < (256LL << 20) &&
                      (file.resize(static_cast<size_t>(len.QuadPart)), true) &&
                      ReadFile(f, file.data(), static_cast<DWORD>(file.size()), &got, nullptr) && got == file.size();
    CloseHandle(f);
    if (!read || file.size() < sizeof(IMAGE_DOS_HEADER))
        return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > file.size())
        return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(file.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != live->FileHeader.TimeDateStamp ||
        nt->OptionalHeader.SizeOfImage != live->OptionalHeader.SizeOfImage)
        return false;
    const u32 size = nt->OptionalHeader.SizeOfImage;
    img.assign(size, 0);
    const u32 hdr = (std::min)(static_cast<u32>(nt->OptionalHeader.SizeOfHeaders),
                               (std::min)(size, static_cast<u32>(file.size())));
    memcpy(img.data(), file.data(), hdr);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    if (reinterpret_cast<const u8*>(sec + nt->FileHeader.NumberOfSections) > file.data() + file.size())
        return false;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec)
    {
        const u32 n = (std::min)(sec->SizeOfRawData, sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData);
        if (!n)
            continue;
        if (sec->VirtualAddress + static_cast<uint64_t>(n) > size || sec->PointerToRawData + static_cast<uint64_t>(n) > file.size())
            return false;
        memcpy(img.data() + sec->VirtualAddress, file.data() + sec->PointerToRawData, n);
    }
    const u32 delta = static_cast<u32>(reinterpret_cast<uintptr_t>(m) - nt->OptionalHeader.ImageBase);
    if (!delta)
        return true;
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!dir.VirtualAddress || !dir.Size || dir.VirtualAddress + static_cast<uint64_t>(dir.Size) > size)
        return false; // moved without relocations: the pointers cannot be put right
    for (u32 at = dir.VirtualAddress; at + sizeof(IMAGE_BASE_RELOCATION) <= dir.VirtualAddress + dir.Size;)
    {
        const auto block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(img.data() + at);
        if (block->SizeOfBlock < sizeof(IMAGE_BASE_RELOCATION) || at + static_cast<uint64_t>(block->SizeOfBlock) > size)
            return false;
        const u32 count = (block->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
        const auto entries = reinterpret_cast<const u16*>(block + 1);
        for (u32 i = 0; i < count; ++i)
        {
            const u32 type = entries[i] >> 12, rva = block->VirtualAddress + (entries[i] & 0xFFF);
            if (type == IMAGE_REL_BASED_ABSOLUTE)
                continue;
            if (type != IMAGE_REL_BASED_HIGHLOW || rva + 4ULL > size)
                return false;
            *reinterpret_cast<u32*>(img.data() + rva) += delta;
        }
        at += block->SizeOfBlock;
    }
    return true;
}

static const IMAGE_NT_HEADERS32* LiveHeaders(HMODULE m)
{
    __try
    {
        auto base = reinterpret_cast<const u8*>(m);
        auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
        return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

// The module's original image (laid out at RVAs like the loaded one), or null when refused.
static const OrigImage* Original(HMODULE m)
{
    const IMAGE_NT_HEADERS32* live = m ? LiveHeaders(m) : nullptr;
    if (!live)
        return nullptr;
    const u32 stamp = live->FileHeader.TimeDateStamp, size = live->OptionalHeader.SizeOfImage;
    AcquireSRWLockExclusive(&g_origLock);
    OrigImage* found = nullptr;
    for (OrigImage* o : g_orig)
        if (o->m == m && o->stamp == stamp && o->size == size)
            found = o;
    if (!found)
    {
        found = new OrigImage{m, stamp, size, {}};
        char buf[MAX_PATH];
        if (!LoadOriginal(m, live, found->img))
        {
            found->img.clear();
            Log("hooks: %s's original image could not be read from its file: what other mods patched there stays "
                "refused", BaseName(m, buf, sizeof(buf)));
        }
        g_orig.push_back(found); // kept for the process (a few MB at most: the game DLLs other mods patch)
    }
    ReleaseSRWLockExclusive(&g_origLock);
    return found->img.empty() ? nullptr : found;
}

const u8* OriginalAt(uintptr_t addr, size_t len)
{
    HMODULE m = ModuleOf(addr);
    const OrigImage* o = Original(m);
    if (!o)
        return nullptr;
    const uintptr_t rva = addr - reinterpret_cast<uintptr_t>(m);
    if (rva + len > o->img.size())
        return nullptr;
    return o->img.data() + rva;
}

bool OriginalPointer(uintptr_t addr, uintptr_t* value)
{
    const u8* p = OriginalAt(addr, 4);
    if (!p || !value)
        return false;
    *value = *reinterpret_cast<const u32*>(p);
    return true;
}

bool IsRestoreStyle(uintptr_t addr)
{
    for (const auto& r : g_rsRanges)
        if (addr >= r.first && addr < r.second)
            return true;
    HMODULE m = ModuleOf(addr);
    if (!m)
        return false;
    static struct
    {
        HMODULE m;
        bool rs;
    } cache[32];
    static int count = 0;
    for (int i = 0; i < count; ++i)
        if (cache[i].m == m)
            return cache[i].rs;
    char buf[MAX_PATH];
    const bool rs = _stricmp(BaseName(m, buf, sizeof(buf)), "fluf.dll") == 0 || ImportsDll(m, "fluf.dll");
    if (count < 32)
        cache[count++] = {m, rs};
    return rs;
}

// ------------------------------------------------------------------ detours
struct Installed
{
    uintptr_t site;  // the hooked function
    uintptr_t at;    // where FLvibed's JMP is (the site, or a restore-style detour's handler)
    u8* stubIn;      // the JMP's target: jmp [ptr] -> dest (a pass-through to `tramp` once moved)
    u8* stubOut;     // what the caller holds as "the original": jmp [ptr] -> tramp
    void* dest;      // the hook
    u8* tramp;       // the code that continues the chain
    HookMode mode;
    char name[40];
    bool insideLogged; // "stays inside FLUF's detour" said once
};
static std::vector<Installed> g_hooks;

static u8* MakeStub(const void* to)
{
    u8* s = static_cast<u8*>(AllocExec(16));
    if (!s)
        return nullptr;
    s[0] = 0xFF; // jmp dword ptr [s+8]
    s[1] = 0x25;
    *reinterpret_cast<u32*>(s + 2) = static_cast<u32>(reinterpret_cast<uintptr_t>(s + 8));
    *reinterpret_cast<const void**>(s + 8) = to;
    return s;
}

static void StubSet(u8* stub, const void* to)
{
    InterlockedExchange(reinterpret_cast<volatile LONG*>(stub + 8), static_cast<LONG>(reinterpret_cast<uintptr_t>(to)));
}

const char* HookModeName(HookMode mode)
{
    switch (mode)
    {
    case HookDirect: return "direct";
    case HookChained: return "chained";
    case HookAtHandler: return "at FLUF's handler";
    default: return "none";
    }
}

HookMode ModeOf(uintptr_t target)
{
    for (size_t i = g_hooks.size(); i-- > 0;)
        if (g_hooks[i].site == target)
            return g_hooks[i].mode;
    return HookNone;
}

// Hook `site` (see Detour): fills `e` (without stubOut) or returns false (logged).
static bool HookAt(uintptr_t site, void* dest, size_t stolen, const char* name, const char* feature, Installed& e)
{
    // Where: follow JMPs into restore-style code (FLUF's handler, through any thunk); stop at a JMP elsewhere (chain
    // to it there) or at plain code.
    uintptr_t at = site, previous = 0;
    bool handler = false;
    for (int depth = 0;; ++depth)
    {
        uintptr_t to = 0;
        if (!ForeignJumpLength(reinterpret_cast<const u8*>(at), &to))
            break;
        if (depth >= 8 || !IsRestoreStyle(to))
        {
            previous = to;
            break;
        }
        at = to;
        handler = true;
    }
    u8* tramp = static_cast<u8*>(AllocExec(64));
    u8* in = MakeStub(dest);
    if (!tramp || !in)
    {
        Log("Detour 0x%08X: out of trampoline memory", static_cast<u32>(site));
        return false;
    }
    size_t patchLen;
    HookMode mode;
    if (previous)
    {
        WriteJmp(tramp, previous); // someone hooked here: chain to their hook
        patchLen = 5;
        mode = handler ? HookAtHandler : HookChained;
        if (!handler)
            Log("Detour 0x%08X: chaining existing hook -> 0x%08X", static_cast<u32>(site), static_cast<u32>(previous));
    }
    else if (!handler)
    {
        memcpy(tramp, reinterpret_cast<const void*>(site), stolen);
        WriteJmp(tramp + stolen, site + stolen);
        patchLen = stolen;
        mode = HookDirect;
    }
    else
    {
        char why[96] = {};
        patchLen = MovePrologue(reinterpret_cast<const u8*>(at), 5, tramp, why, sizeof(why));
        if (!patchLen)
        {
            Log("Hook %s NOT installed: FLUF owns the function (a detour that re-writes its jump on every call, %s) "
                "and its handler's start cannot be moved (%s) - %s off",
                name ? name : "?", Where(at).c_str(), why, feature ? feature : "its feature");
            return false;
        }
        mode = HookAtHandler;
    }
    if (handler)
        Log("Hook %s: FLUF owns the function (a detour that re-writes its jump on every call) - hooked at its handler "
            "%s (%s): both run, FLvibed first",
            name ? name : "?", Where(at).c_str(),
            previous ? "after FLvibed's hook already there" : (std::to_string(patchLen) + " bytes moved").c_str());
    u8 patch[32];
    memset(patch, 0x90, sizeof(patch));
    patch[0] = 0xE9;
    *reinterpret_cast<i32*>(patch + 1) = static_cast<i32>(reinterpret_cast<uintptr_t>(in) - (at + 5));
    if (patchLen > sizeof(patch) || !Write(at, patch, patchLen))
    {
        Log("Detour 0x%08X: write failed", static_cast<u32>(at));
        return false;
    }
    e = {};
    e.site = site;
    e.at = at;
    e.stubIn = in;
    e.dest = dest;
    e.tramp = tramp;
    e.mode = mode;
    if (name)
        strncpy_s(e.name, name, _TRUNCATE);
    else
        _snprintf_s(e.name, sizeof(e.name), _TRUNCATE, "0x%08X", static_cast<u32>(site));
    return true;
}

void* Detour(uintptr_t target, void* dest, size_t stolen, const char* name, const char* feature)
{
    Installed e;
    if (!HookAt(target, dest, stolen, name, feature, e))
        return nullptr;
    e.stubOut = MakeStub(e.tramp);
    if (!e.stubOut)
        return nullptr;
    g_hooks.push_back(e);
    return e.stubOut;
}

static volatile LONG g_adoptPending = 0;

bool NoteCaller(const void* ret)
{
    if (!IsRestoreStyle(reinterpret_cast<uintptr_t>(ret)))
        return false;
    InterlockedExchange(&g_adoptPending, 1);
    return true;
}

bool AdoptPending() { return InterlockedExchange(&g_adoptPending, 0) != 0; }

int AdoptRestoreStyle(const char* who)
{
    int moved = 0, stayed = 0, atHandler = 0, newlyInside = 0;
    static int runs = 0;
    for (Installed& e : g_hooks)
    {
        if (e.at == e.site)
        {
            const u8* c = reinterpret_cast<const u8*>(e.site);
            uintptr_t to = 0;
            ForeignJumpLength(c, &to);
            if (to && IsRestoreStyle(to))
            {
                // FLUF hooked this after FLvibed: FLvibed's JMP is its saved "original" (it restores it for every
                // call, so FLvibed's hook runs inside FLUF's, with FLUF as the caller). Move the hook to FLUF's
                // handler; the old JMP passes through to what followed it.
                Installed n;
                if (HookAt(e.site, e.dest, 5, e.name, nullptr, n))
                {
                    StubSet(e.stubIn, e.tramp);
                    StubSet(e.stubOut, n.tramp);
                    e.at = n.at;
                    e.stubIn = n.stubIn;
                    e.tramp = n.tramp;
                    e.mode = n.mode;
                    ++moved;
                }
                else
                {
                    ++stayed;
                    if (!e.insideLogged)
                    {
                        e.insideLogged = true;
                        ++newlyInside;
                        Log("%s: hook %s stays inside FLUF's detour (both run; its caller reads as FLUF)", who, e.name);
                    }
                }
            }
        }
        atHandler += e.mode == HookAtHandler;
    }
    HMODULE fluf = GetModuleHandleA("fluf.dll");
    if (runs++ && !moved && !newlyInside)
        return 0; // a later check that changed nothing stays quiet
    if (!fluf && !atHandler && !stayed)
        Log("%s: no FLUF (checked at the first frame)", who);
    else
        Log("%s: FLUF present%s: %d hook(s) at its handlers (%d moved there now), %d inside its detour", who,
            fluf ? "" : " (a restore-style detour)", atHandler, moved, stayed);
    return moved;
}

void HookReport(char* out, size_t len)
{
    std::string r;
    for (const Installed& e : g_hooks)
    {
        const u8* c = reinterpret_cast<const u8*>(e.site);
        uintptr_t to = 0;
        ForeignJumpLength(c, &to);
        char line[256];
        _snprintf_s(line, sizeof(line), _TRUNCATE, "%s %s at %s; the function's jump -> %s%s\n", e.name,
                    HookModeName(e.mode), Where(e.at).c_str(), to ? Where(to).c_str() : "(none)",
                    to && IsRestoreStyle(to) ? " (FLUF)" : "");
        r += line;
    }
    strncpy_s(out, len, r.empty() ? "no hooks\n" : r.c_str(), _TRUNCATE);
}

void LogRestoreStyleAtStart(const char* who)
{
    if (GetModuleHandleA("fluf.dll"))
        Log("%s: FLUF loaded before FLvibed: the functions it detours are hooked at its handlers", who);
    else
        Log("%s: FLUF not loaded (yet): checked again at the first frame", who);
}

void* Detour(Sym& target, void* dest, size_t stolen, const char* feature)
{
    std::vector<u8> bytes, mask;
    const bool covered = ParseSig(target.sig, bytes, mask) && target.off + stolen <= bytes.size();
    if (!target.get() || target.kind != Code || !covered)
    {
        Log("Hook %s (v1.1 0x%06X) NOT installed: %s - %s off", target.name, target.va,
            target.kind != Code ? "not code"
                                : (!covered ? "signature shorter than the stolen bytes" : StateName(target.state)),
            feature);
        return nullptr;
    }
    return Detour(target.get(), dest, stolen, target.name, feature);
}

// Matches of a parsed signature in the code sections of a loaded module: count, and the first one. -1 on a fault.
static int ScanModuleCode(const u8* base, const u8* bytes, const u8* mask, size_t len, uintptr_t& first)
{
    int count = 0;
    __try
    {
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
        for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++sec)
        {
            if (!(sec->Characteristics & IMAGE_SCN_CNT_CODE))
                continue;
            const u8* p = base + sec->VirtualAddress;
            const size_t n = sec->Misc.VirtualSize;
            for (size_t i = 0; i + len <= n; ++i)
            {
                if (p[i] != bytes[0])
                    continue;
                size_t k = 1;
                while (k < len && (!mask[k] || p[i + k] == bytes[k]))
                    ++k;
                if (k == len && ++count == 1)
                    first = reinterpret_cast<uintptr_t>(p + i);
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -1;
    }
    return count;
}

uintptr_t FindInModule(const char* module, const char* sig, int off)
{
    std::vector<u8> bytes, mask;
    HMODULE m = GetModuleHandleA(module);
    if (!m || !ParseSig(sig, bytes, mask) || bytes.empty())
        return 0;
    uintptr_t first = 0;
    const int count = ScanModuleCode(reinterpret_cast<const u8*>(m), bytes.data(), mask.data(), bytes.size(), first);
    const uintptr_t found = first + off;
    if (count != 1)
    {
        Log("%s: signature %s %s: refused", module, count ? "found more than once" : "not found", sig);
        return 0;
    }
    return found;
}

uintptr_t FindInModuleOriginal(const char* module, const char* sig, int off)
{
    std::vector<u8> bytes, mask;
    HMODULE m = GetModuleHandleA(module);
    const OrigImage* o = m ? Original(m) : nullptr;
    if (!o || !ParseSig(sig, bytes, mask) || bytes.empty())
        return 0;
    uintptr_t first = 0;
    const int count = ScanModuleCode(o->img.data(), bytes.data(), mask.data(), bytes.size(), first);
    if (count != 1)
    {
        Log("%s: signature %s in the module's file %s: refused", module, count ? "found more than once" : "not found", sig);
        return 0;
    }
    return reinterpret_cast<uintptr_t>(m) + (first - reinterpret_cast<uintptr_t>(o->img.data())) + off;
}

static bool MatchRaw(const u8* at, const u8* bytes, const u8* mask, size_t len)
{
    __try
    {
        for (size_t i = 0; i < len; ++i)
            if (mask[i] && at[i] != bytes[i])
                return false;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

u32 ExportOperand(const char* module, const char* exportName, const char* sig, int off)
{
    std::vector<u8> bytes, mask;
    HMODULE m = GetModuleHandleA(module);
    const u8* fn = m ? reinterpret_cast<const u8*>(GetProcAddress(m, exportName)) : nullptr;
    if (!fn || !ParseSig(sig, bytes, mask) || off < 0 || static_cast<size_t>(off) + 4 > bytes.size() ||
        !MatchRaw(fn, bytes.data(), mask.data(), bytes.size()))
    {
        Log("%s: %s does not match %s: its operand refused", module, exportName, sig);
        return 0;
    }
    return Mem<u32>(reinterpret_cast<uintptr_t>(fn) + off);
}

static bool FirstPointerIs(const void* obj, const void* vt)
{
    __try
    {
        return *static_cast<const void* const*>(obj) == vt;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool IsExportedClass(const void* obj, const char* module, const char* vtableExport)
{
    if (!obj)
        return false;
    // export name -> its address (null: not exported here, logged once)
    static std::map<std::string, const void*> cache;
    const std::string key = std::string(module) + "!" + vtableExport;
    auto it = cache.find(key);
    if (it == cache.end())
    {
        HMODULE m = GetModuleHandleA(module);
        if (!m)
            return false; // not loaded yet: ask again later
        const void* vt = reinterpret_cast<const void*>(GetProcAddress(m, vtableExport));
        if (!vt)
            Log("%s: vtable %s not exported: objects of that class refused", module, vtableExport);
        it = cache.emplace(key, vt).first;
    }
    return it->second && FirstPointerIs(obj, it->second);
}

uintptr_t FindModuleData(const char* module, const char* sig, int off)
{
    const uintptr_t at = FindInModule(module, sig, off);
    return at ? Mem<u32>(at) : 0;
}

// The slot of `method` in the exported `vtable`: entries are read while they point into the module (a vtable ends
// where its entries stop being the module's code), at most 512. `original`: the entries as the module's file has them.
static int FindSlotIn(HMODULE m, void* const* vt, const void* method, bool original)
{
    const auto base = reinterpret_cast<uintptr_t>(m);
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const uintptr_t end = base + nt->OptionalHeader.SizeOfImage;
    __try
    {
        for (int i = 0; i < 512; ++i)
        {
            uintptr_t e = reinterpret_cast<uintptr_t>(vt[i]);
            if (original && !OriginalPointer(reinterpret_cast<uintptr_t>(&vt[i]), &e))
                return -1;
            if (e < base || e >= end)
                return -1;
            if (e == reinterpret_cast<uintptr_t>(method))
                return i;
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }
    return -1;
}

int VtableSlot(HMODULE m, void* const* vt, const void* method)
{
    if (!m || !vt || !method)
        return -1;
    const int live = FindSlotIn(m, vt, method, false);
    if (live >= 0)
        return live;
    // Another mod may have rewritten an entry before the method's (FLUF's VTableHook on CShip, CELauncher ...: the live
    // vtable "ends" at its pointer): the module's file has the class's own entries; calls through the live slot then
    // run the other mod's hook, which chains to the game's code.
    return FindSlotIn(m, vt, method, true);
}
static int FindSlot(HMODULE m, void* const* vt, const void* method) { return VtableSlot(m, vt, method); }

int Virtual::Slot()
{
    if (slot != -2)
        return slot;
    slot = -1;
    HMODULE m = GetModuleHandleA(module);
    void* const* vt = m ? reinterpret_cast<void* const*>(GetProcAddress(m, vtable)) : nullptr;
    const void* fn = m ? reinterpret_cast<const void*>(GetProcAddress(m, method)) : nullptr;
    if (vt && fn)
        slot = FindSlot(m, vt, fn);
    if (slot < 0)
        Log("%s: virtual %s not found in %s (%s): calls refused", module, method, vtable,
            !m    ? "module not loaded"
            : !vt ? "no vtable export"
            : !fn ? "no method export"
                  : "not in the vtable");
    return slot;
}

void* PatchVtable(void** vtable, int index, void* fn)
{
    void* old = vtable[index];
    Write(reinterpret_cast<uintptr_t>(&vtable[index]), &fn, sizeof(fn));
    return old;
}

void* PatchImport(HMODULE module, const char* dllName, const char* funcName, void* fn)
{
    void** slot = ImportSlot(module, dllName, funcName);
    if (!slot)
        return nullptr;
    void* old = *slot;
    Write(reinterpret_cast<uintptr_t>(slot), &fn, sizeof(fn));
    return old;
}

void** ImportSlot(HMODULE module, const char* dllName, const char* funcName)
{
    if (!module)
        return nullptr;
    auto base = reinterpret_cast<u8*>(module);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress)
        return nullptr;

    for (auto imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name; ++imp)
    {
        if (_stricmp(reinterpret_cast<char*>(base + imp->Name), dllName) != 0)
            continue;
        auto names = reinterpret_cast<IMAGE_THUNK_DATA32*>(
            base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto iat = reinterpret_cast<IMAGE_THUNK_DATA32*>(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++iat)
        {
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
                continue;
            auto byName = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (strcmp(reinterpret_cast<char*>(byName->Name), funcName) != 0)
                continue;
            return reinterpret_cast<void**>(&iat->u1.Function);
        }
    }
    return nullptr;
}
} // namespace hk
