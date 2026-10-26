// sigtool - Freelancer.exe signatures for core\flexe.h (build.cmd sigtool -> build\sigtool\sigtool.exe).
//   sigtool gen <exe> code <va> [min length]   signature for code at <va> (function entry: min length = the
//                                              detour's stolen bytes; the signature then starts at <va>)
//   sigtool gen <exe> data <va>                signature of an instruction that references the global <va>
//   sigtool check <exe> [--scan] [--shift N] [--hooked]
//                                              resolve every symbol of core\flexe.h in <exe> (fast path / scan);
//                                              the options test the search (see Check)
// Signatures: bytes that look like addresses (a value inside the image, the rel32 of E8/E9/0F 8x) are wildcards,
// so a relinked build still matches; the shortest window that is unique in .text wins. Code signatures stay
// unique with their first 8 bytes ignored (another plugin's JMP there must not make them ambiguous); data
// signatures avoid the first 8 bytes of every code symbol (FLvibed's own detours overwrite them).
#include "pre.h"

#include "flexe.h"
#include "log.h"

static std::vector<u8> g_file;
static std::vector<u8> g_mem;
static std::vector<u8> g_wild; // per .text byte: looks like an address
static hk::Image g_img;

static bool LoadImage(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    g_file.resize(ftell(f));
    fseek(f, 0, SEEK_SET);
    fread(g_file.data(), 1, g_file.size(), f);
    fclose(f);
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_file.data());
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(g_file.data() + dos->e_lfanew);
    g_mem.assign(nt->OptionalHeader.SizeOfImage, 0);
    memcpy(g_mem.data(), g_file.data(), nt->OptionalHeader.SizeOfHeaders);
    g_img = {};
    g_img.base = nt->OptionalHeader.ImageBase;
    g_img.size = nt->OptionalHeader.SizeOfImage;
    auto sec = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
    {
        const u32 n =
            std::min<u32>(sec->SizeOfRawData, sec->Misc.VirtualSize ? sec->Misc.VirtualSize : sec->SizeOfRawData);
        memcpy(g_mem.data() + sec->VirtualAddress, g_file.data() + sec->PointerToRawData, n);
        if (!g_img.textVa && (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE))
        {
            g_img.textVa = g_img.base + sec->VirtualAddress;
            g_img.textSize = sec->Misc.VirtualSize;
        }
    }
    g_img.mem = g_mem.data();
    hk::SetImage(g_img);
    return true;
}

static const u8* At(u32 va) { return g_mem.data() + (va - g_img.base); }

static void MarkWildcards()
{
    const u8* t = At(g_img.textVa);
    const u32 n = g_img.textSize;
    g_wild.assign(n + 8, 0);
    for (u32 i = 0; i + 4 < n; ++i)
    {
        auto wild4 = [&](u32 from)
        {
            for (u32 k = from; k < from + 4 && k < n; ++k)
                g_wild[k] = 1;
        };
        if (t[i] == 0xE8 || t[i] == 0xE9)
            wild4(i + 1);
        if (t[i] == 0x0F && i + 5 < n && t[i + 1] >= 0x80 && t[i + 1] <= 0x8F)
            wild4(i + 2);
        const u32 v = *reinterpret_cast<const u32*>(t + i);
        if (v >= g_img.base && v < g_img.base + g_img.size)
            wild4(i);
    }
}

// Literal (non-wildcard) bytes a signature needs besides the ones it may ignore: a short pattern could match some
// unrelated code in another build.
constexpr int kMinLiterals = 12;

struct Cand
{
    u32 start = 0;
    u32 len = 0;
};

// Pattern [start, start+len) of the image; `ignoreFrom/To` (relative) are extra wildcards for the uniqueness test.
static void Build(u32 start, u32 len, std::vector<u8>& bytes, std::vector<u8>& mask)
{
    bytes.assign(At(start), At(start) + len);
    mask.assign(len, 1);
    for (u32 i = 0; i < len; ++i)
        if (g_wild[start - g_img.textVa + i])
            mask[i] = 0;
}

static int CountMatches(const std::vector<u8>& bytes, const std::vector<u8>& mask, int limit)
{
    size_t anchor = 0;
    while (anchor < mask.size() && !mask[anchor])
        ++anchor;
    if (anchor == mask.size())
        return limit;
    const u8* t = At(g_img.textVa);
    const u32 n = g_img.textSize;
    int count = 0;
    for (u32 i = static_cast<u32>(anchor); i + (bytes.size() - anchor) <= n;)
    {
        const void* hit = memchr(t + i, bytes[anchor], n - i);
        if (!hit)
            break;
        const u32 pos = static_cast<u32>(static_cast<const u8*>(hit) - t);
        if (pos < anchor)
        {
            i = pos + 1;
            continue;
        }
        const u32 st = pos - static_cast<u32>(anchor);
        if (st + bytes.size() <= n)
        {
            bool ok = true;
            for (size_t k = 0; k < bytes.size() && ok; ++k)
                ok = !mask[k] || t[st + k] == bytes[k];
            if (ok && ++count >= limit)
                return count;
        }
        i = pos + 1;
    }
    return count;
}

static int Literals(const std::vector<u8>& mask)
{
    int n = 0;
    for (u8 m : mask)
        n += m;
    return n;
}

// Is [start, start+len) unique with [ign0, ign1) (relative) also ignored, and specific enough?
static bool Good(u32 start, u32 len, int ign0, int ign1, int minLiterals)
{
    if (start < g_img.textVa || start + len > g_img.textVa + g_img.textSize)
        return false;
    std::vector<u8> bytes, mask;
    Build(start, len, bytes, mask);
    for (int i = std::max(0, ign0); i < ign1 && i < static_cast<int>(len); ++i)
        mask[i] = 0;
    return Literals(mask) >= minLiterals && CountMatches(bytes, mask, 2) == 1;
}

// Shortest good length for a window starting at `start` (binary search: uniqueness grows with length).
static u32 Shortest(u32 start, u32 minLen, u32 maxLen, int ign0, int ign1, int minLiterals)
{
    if (!Good(start, maxLen, ign0, ign1, minLiterals))
        return 0;
    u32 lo = minLen, hi = maxLen;
    while (lo < hi)
    {
        const u32 mid = (lo + hi) / 2;
        if (Good(start, mid, ign0, ign1, minLiterals))
            hi = mid;
        else
            lo = mid + 1;
    }
    return lo;
}

static std::string Pattern(u32 start, u32 len)
{
    std::vector<u8> bytes, mask;
    Build(start, len, bytes, mask);
    std::string s;
    char hex[4];
    for (u32 i = 0; i < len; ++i)
    {
        if (i)
            s += ' ';
        if (mask[i])
        {
            sprintf_s(hex, "%02X", bytes[i]);
            s += hex;
        }
        else
            s += "??";
    }
    return s;
}

static bool OverlapsCodeEntry(u32 start, u32 len)
{
    for (hk::Sym* s : exe::kAll)
        if (s->kind == hk::Code && start < s->va + 8 && s->va < start + len)
            return true;
    return false;
}

static int GenCode(u32 va, u32 minLen)
{
    // A function entry starts its signature at the entry when that is unique (the fast path then checks the bytes
    // a detour steals); otherwise, and for addresses inside a function, the window may start up to 24 bytes before.
    Cand best;
    for (u32 back = 0; back <= 24 && !(minLen && best.len && back > 0); ++back)
    {
        const u32 start = va - back;
        const u32 len = Shortest(start, std::max<u32>({back + minLen, back + 8, 12}), 96, back, back + 8, kMinLiterals);
        if (len && (!best.len || len < best.len))
            best = {start, len};
    }
    if (!best.len)
    {
        printf("no unique signature for code 0x%06X\n", va);
        return 1;
    }
    printf("Code 0x%06X 0 %u \"%s\"\n", va, va - best.start, Pattern(best.start, best.len).c_str());
    return 0;
}

static int GenData(u32 va)
{
    const u8* t = At(g_img.textVa);
    Cand best;
    u32 bestSite = 0;
    int sites = 0;
    for (u32 i = 0; i + 4 <= g_img.textSize && sites < 64; ++i)
    {
        if (*reinterpret_cast<const u32*>(t + i) != va)
            continue;
        ++sites;
        const u32 site = g_img.textVa + i;
        for (u32 back = 1; back <= 12; ++back)
        {
            const u32 start = site - back;
            const u32 len = Shortest(start, back + 4, 64, -1, -1, kMinLiterals);
            if (len && !OverlapsCodeEntry(start, len) && (!best.len || len < best.len))
            {
                best = {start, len};
                bestSite = site;
            }
        }
    }
    if (!best.len)
    {
        printf("no unique signature for data 0x%06X (%d references)\n", va, sites);
        return 1;
    }
    printf("Data 0x%06X 0x%06X %u \"%s\"\n", va, bestSite, bestSite - best.start,
           Pattern(best.start, best.len).c_str());
    return 0;
}

static void PrintSink(const char* line) { printf("%s\n", line); }

static const char* kUsage = "usage: sigtool gen <exe> code <va> [min length] | gen <exe> data <va> |\n"
                            "       sigtool check <exe> [--scan] [--shift N] [--hooked]\n";

// check [--scan] [--shift N] [--hooked]: resolve the table. --scan skips the v1.1 fast path; --shift moves the
// whole .text N bytes up (a stand-in for another build: every code address moves, the globals stay) and
// --hooked puts a detour JMP on every function entry (other plugins' hooks). Each symbol must then be found at
// its expected address.
static int Check(int argc, char** argv)
{
    bool scan = false, hooked = false;
    u32 shift = 0;
    for (int i = 3; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--scan"))
            scan = true;
        else if (!strcmp(argv[i], "--hooked"))
            hooked = true;
        else if (!strcmp(argv[i], "--shift") && i + 1 < argc)
            shift = strtoul(argv[++i], nullptr, 0);
    }
    if (shift)
    {
        u8* text = g_mem.data() + (g_img.textVa - g_img.base);
        const u32 room = ((g_img.textSize + 0xFFF) & ~0xFFFu) - g_img.textSize;
        if (shift > room)
        {
            printf("--shift %u: only %u bytes free after .text\n", shift, room);
            return 2;
        }
        memmove(text + shift, text, g_img.textSize);
        memset(text, 0xCC, shift);
        g_img.textSize += shift;
        hk::SetImage(g_img);
    }
    if (hooked)
        for (hk::Sym* s : exe::kAll)
            if (s->kind == hk::Code && s->off == 0)
            {
                u8* at = g_mem.data() + (s->va + shift - g_img.base);
                at[0] = 0xE9;
                *reinterpret_cast<u32*>(at + 1) = 0x12345678;
            }
    hk::SetScanOnly(scan);
    hk::ResolveAll(exe::kAll, sizeof(exe::kAll) / sizeof(exe::kAll[0]), "check");
    int ok = 0, wrong = 0, missing = 0;
    for (hk::Sym* s : exe::kAll)
    {
        const u32 want = s->kind == hk::Code ? s->va + shift : s->va;
        if (!s->addr)
            ++missing;
        else if (s->addr == want)
            ++ok;
        else
        {
            ++wrong;
            printf("WRONG %s: 0x%06X, expected 0x%06X\n", s->name, s->addr, want);
        }
    }
    printf("expected addresses: %d right, %d wrong, %d unresolved\n", ok, wrong, missing);
    return wrong ? 1 : 0;
}

int main(int argc, char** argv)
{
    if (argc < 3 || !LoadImage(argv[2]))
    {
        printf("%s", kUsage);
        return 2;
    }
    LogSetSink(PrintSink, "");
    if (!strcmp(argv[1], "check"))
        return Check(argc, argv);
    if (!strcmp(argv[1], "gen") && argc >= 5)
    {
        MarkWildcards();
        const u32 va = strtoul(argv[4], nullptr, 16);
        if (!strcmp(argv[3], "code"))
            return GenCode(va, argc >= 6 ? atoi(argv[5]) : 0);
        if (!strcmp(argv[3], "data"))
            return GenData(va);
    }
    printf("%s", kUsage);
    return 2;
}
