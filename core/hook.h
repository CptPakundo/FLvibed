#pragma once
#include "common.h"

namespace hk
{
// ---- Freelancer.exe symbols: every address FLvibed hooks, calls or reads is a Sym (table: core\flexe.h) ----
// A Sym is found by its signature (hex bytes, "??" = any byte) and never trusted blindly:
//  - Code: the address is `off` bytes into the signature. Fast path: the signature matches at the known v1.1
//    address (va - off). A function already detoured by another plugin (or by an earlier FLvibed part) counts as
//    matching: when the address holds a JMP (E9), its 5 bytes and the NOPs padding the stolen bytes are skipped.
//  - Data: a global, read from an instruction operand that references it; the operand is `off` bytes into the
//    signature. Fast path: the signature matches at the v1.1 instruction (site - off) and the operand is `va`.
//  - Otherwise the signature is searched in Freelancer.exe's .text (once; results cached); exactly one match
//    resolves the Sym (another exe build), none or several leave it unresolved.
// An unresolved Sym fails closed: Detour() refuses the hook, Call() returns R(), Mem() reads/writes a scratch
// value, is() is false. Only the feature that needs it stops working, and the module log says which.
// Signatures come from tools\sigtool (build.cmd sigtool), see docs\FREELANCER_RE.md "Signatures".
enum SymKind : u8
{
    Code,
    Data,
};
enum SymState : u8
{
    SymUnresolved = 0,
    SymV11,       // the known v1.1 address, its bytes verified
    SymScanned,   // found by searching the signature (another Freelancer.exe build)
    SymMissing,   // signature not found: fails closed
    SymAmbiguous, // signature found more than once: fails closed
};
struct Sym
{
    const char* name;
    SymKind kind;
    u32 va;          // v1.1 (HD Edition 0.7.1) address
    u32 site;        // Data: v1.1 address of the operand that holds `va` (0 for Code)
    int off;         // Code: offset of the address in the signature; Data: offset of the operand
    const char* sig; // hex bytes, "??" = any byte
    u32 addr;        // resolved address, 0 = unresolved (filled by Resolve)
    SymState state;

    uintptr_t get();                                 // the resolved address or 0 (resolves on first use)
    bool ok() { return get() != 0; }                 // usable
    bool is(uintptr_t v) { return v && v == get(); } // identity test (vtables, handlers, return addresses)
    template <class F> F fn() { return reinterpret_cast<F>(get()); } // typed function pointer or null
};

// Resolve `count` symbols now (the hosts and logics call it once at startup) and log a summary plus every symbol
// that is not at its v1.1 address. `who` prefixes the log lines.
void ResolveAll(Sym* const* syms, size_t count, const char* who);
// Resolve one symbol (cached; ResolveAll and Sym::get call it).
bool Resolve(Sym& s);
const char* StateName(SymState state);

// The image the symbols are resolved in: Freelancer.exe by default; tools\sigtool points it at a file.
struct Image
{
    const u8* mem;        // where VA `base` is mapped
    u32 base;             // image base VA (Freelancer.exe: 0x400000)
    u32 textVa, textSize; // code section
    u32 size;             // SizeOfImage
};
void SetImage(const Image& image);
const Image& GetImage();
// Test switch: skip the v1.1 fast path (every symbol found by its signature). Also on when FLvibed is built with
// FLV_SIG_SCAN_ONLY defined.
void SetScanOnly(bool on);
// Does the signature match with its start at VA `at`? `hookAt` = index of a code address inside the signature
// (another plugin's JMP may sit there) or -1.
bool MatchAt(const u8* pattern, const u8* mask, size_t len, u32 at, int hookAt);
// Parse "51 55 ?? 33" into bytes + mask (1 = literal). False on a malformed signature.
bool ParseSig(const char* sig, std::vector<u8>& bytes, std::vector<u8>& mask);

// ---- Virtual functions of the game's DLL interfaces, by name: no hard-coded vtable slot numbers (core\fldll.h) ----
// A Virtual names a method by its exported (mangled) name and the vtable it belongs to by that vtable's own export
// (Common.dll exports both, e.g. ??_7IObjInspectImpl@@6BIObjInspect@@@ and ?get_id@IObjInspectImpl@@UBE?BIXZ). Its
// slot is the index where the exported vtable holds the method's export (scanned once, cached), so it follows the
// game build. The mangled name fixes the exact signature (parameters; a class returned by value = a hidden pointer
// argument): the call type next to each Virtual is written from it. Unknown (an export missing, the method not in that
// vtable, e.g. patched by another plugin) = slot -1: get() returns null and callers fail closed (logged once).
struct Virtual
{
    const char* module; // the DLL that exports both, e.g. "common.dll"
    const char* vtable; // the vtable's export (??_7...)
    const char* method; // the method's export
    int slot = -2;      // -2 not looked up yet, -1 unknown

    int Slot();
    // The function in `obj`'s own vtable at that slot (an override in a derived class keeps the slot), or null.
    template <class F> F get(const void* obj)
    {
        const int s = Slot();
        if (s < 0 || !obj)
            return nullptr;
        return reinterpret_cast<F>((*static_cast<void* const* const*>(obj))[s]);
    }
};

// Is `obj` an instance of the class whose vtable `module` exports (`??_7Class@@6B@`): its first pointer is that export.
// For checking a game DLL object's class before reading its fields by offset (fail closed on anything else); a derived
// class has its own vtable, so callers list every class they accept. Lookups are cached per export name.
bool IsExportedClass(const void* obj, const char* module, const char* vtableExport);

// ---- Data and code inside the game's other DLLs (Common.dll, ...), by signature: no fixed module offsets ----
// The address of the unique match of `sig` in `module`'s code sections (as loaded: relocated, possibly patched by
// other mods) plus `off`; 0 when the module is not loaded or the pattern is missing or found more than once (logged).
// The file differs between vanilla v1.0, HD Edition and mods (vanilla's dock range float sits 0x1000 lower).
uintptr_t FindInModule(const char* module, const char* sig, int off);
// A data address read from the 32-bit operand at `off` in that unique match (an instruction that references it).
uintptr_t FindModuleData(const char* module, const char* sig, int off);

// ---- What another mod patched at run time: the module's original image, read from its own file (26 Oct 2026) ----
// FLUF rewrites vtable slots (every control class's vt[0xA8], Common.dll classes, IServerImpl), instructions (the
// lane code) and pointers; the loaded module's file says what the game had there. Loaded on first use per module (the
// Windows loader's layout, relocated to the live base), refused (null / 0 / false, logged once) when the file is not
// the loaded image (TimeDateStamp / SizeOfImage) or cannot be read. Callers ask only after the live read failed, so a
// game without such a mod never reads a file.
// The original bytes at a live address inside a loaded module (null outside one / refused).
const u8* OriginalAt(uintptr_t addr, size_t len);
// The original 32-bit pointer at a live address (relocated to the live base).
bool OriginalPointer(uintptr_t addr, uintptr_t* value);
// Like FindInModule over the module's original code: the LIVE address of the unique match + `off`.
uintptr_t FindInModuleOriginal(const char* module, const char* sig, int off);
// The slot of `method` in vtable `vt` of module `m`: the live entries first; when they end at another module's
// pointer before the method, the file's entries (the class's own layout). -1 when not found.
int VtableSlot(HMODULE m, void* const* vt, const void* method);
// A 32-bit operand inside an exported function (a field offset, a constant), taken from the game's own code: `sig`
// must match at the export's address (logged and 0 otherwise); the value is the u32 at `off`. E.g. a struct field's
// offset from an accessor that reads it, so the offset follows the DLL build instead of being hard-coded.
u32 ExportOperand(const char* module, const char* exportName, const char* sig, int off);

// Call a symbol through function pointer type F; returns R() (nothing called) when it is unresolved.
template <class F, class... A> auto Call(Sym& s, A... args) -> decltype(F()(args...))
{
    typedef decltype(F()(args...)) R;
    F f = s.fn<F>();
    if (!f)
        return R();
    return f(args...);
}

// Write bytes to (possibly read-only) code or data.
bool Write(uintptr_t addr, const void* data, size_t len);

// Executable memory for trampolines.
void* AllocExec(size_t len);

// Redirect `target` to `dest` with a 5-byte JMP. `stolen` is the number of whole
// instruction bytes (>= 5, no relative branches) copied into the returned trampoline,
// which runs the original code. If the target already starts with a JMP (another
// plugin's hook), the trampoline chains to that hook instead.
// Restore-style detours (FLUF / FLSDK's FunctionDetour: they put the saved bytes back for every call of the original
// and re-write their own jump afterwards, so a jump chained over theirs is erased by their first call): when the
// target's JMP leads into such a module, FLvibed hooks the handler that JMP goes to instead (its first whole
// instructions moved into the trampoline, relative calls / jumps relocated) - code that module never rewrites - so
// both run, FLvibed outermost. A handler whose first instructions cannot be moved is refused (null, logged).
// The JMP FLvibed writes leads to a small stub (FLvibed's own pool) and the returned pointer is a stub too, so a hook
// can later move (AdoptRestoreStyle) without the caller's saved "original" pointer changing.
// `name` / `feature` only label the log lines.
void* Detour(uintptr_t target, void* dest, size_t stolen, const char* name = nullptr, const char* feature = nullptr);

// ---- Co-residency with restore-style detours (FLUF) ----
enum HookMode : u8
{
    HookNone = 0,  // not hooked by this module part
    HookDirect,    // FLvibed's JMP at the target, the stolen bytes in the trampoline
    HookChained,   // FLvibed's JMP at the target, the trampoline jumps to the plugin hooked there before
    HookAtHandler, // the target belongs to a restore-style detour: FLvibed hooks its handler (both run)
};
// How this module part hooked `target` (the latest Detour on it).
HookMode ModeOf(uintptr_t target);
const char* HookModeName(HookMode mode);
// Is `addr` code of a restore-style detourer: FLUF.dll or a module that imports it (FLUF's modules), or a range the
// tests registered. Cached per module.
bool IsRestoreStyle(uintptr_t addr);
void AddRestoreStyleRange(uintptr_t begin, uintptr_t end); // tests: a simulated FLUF
void ClearRestoreStyleRanges();
// Once the other DLLs are loaded (the hosts call it once, at their first frame): every hook of this module part whose
// target a restore-style detour took AFTER FLvibed (FLUF loaded later: FLvibed's JMP is then its saved "original",
// so FLvibed's hook ran inside FLUF's, with FLUF as its caller) moves to that detour's handler: the old JMP becomes a
// pass-through, the hook's "original" pointer leads into FLUF's handler. Logs one line per moved hook and a summary
// ("no FLUF" when there is none). Returns the hooks moved.
int AdoptRestoreStyle(const char* who);
// The event for a FLUF detour installed after the first frame (FLUF builds some of its hooks late, e.g. with its UI): a
// hook whose caller matters passes its return address here; true (and an adoption becomes pending) when the caller is
// restore-style code - FLvibed's hook then runs inside FLUF's call. The hosts run AdoptRestoreStyle again, outside
// that call, when AdoptPending() says so (it clears the flag).
bool NoteCaller(const void* ret);
bool AdoptPending();
// Every hook of this module part: name, mode, where FLvibed's jump is, where the function's own first jump leads now
// (debug `hooks`).
void HookReport(char* out, size_t len);
// One startup line: FLUF loaded before this module part, or not (yet).
void LogRestoreStyleAtStart(const char* who);
// The length of the x86-32 instruction at `code` if a hook may move it into a trampoline (relocating E8 / E9 rel32
// and EB rel8), else 0 (relative conditional jumps, far transfers, int3 padding, unknown opcodes). `terminal`: an
// unconditional transfer (ret, jmp).
size_t MovableInsnLength(const u8* code, bool* terminal = nullptr);
// Detour a Freelancer.exe symbol. Refused (null, logged with `feature`, the part of FLvibed that stays off) when
// the symbol is unresolved, not code, or its signature does not cover the `stolen` bytes.
void* Detour(Sym& target, void* dest, size_t stolen, const char* feature);

// Replace one entry of a COM vtable; returns the previous function pointer.
void* PatchVtable(void** vtable, int index, void* fn);

// The import address table slot of `funcName` (imported by name from `dllName`) in `module`, or null.
void** ImportSlot(HMODULE module, const char* dllName, const char* funcName);

// Replace an imported function in `module`'s import address table.
// Returns the previous pointer, or null if the import was not found.
void* PatchImport(HMODULE module, const char* dllName, const char* funcName, void* fn);
} // namespace hk

// Mem() on a symbol (plus a byte offset): an unresolved symbol reads zero and swallows writes.
template <class T> inline T& Mem(hk::Sym& s, uintptr_t offset = 0)
{
    const uintptr_t a = s.get();
    if (!a)
    {
        static T scratch;
        scratch = T();
        return scratch;
    }
    return Mem<T>(a + offset);
}

// Call a COM method by vtable index (__stdcall, `this` first).
template <typename R, typename... A> inline R VCall(void* obj, int index, A... args)
{
    typedef R(__stdcall * Fn)(void*, A...);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(obj))[index])(obj, args...);
}
