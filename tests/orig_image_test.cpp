// hk's original-image reads (26 Oct 2026, the FLUF fail-closed fixes): a vtable entry another mod rewrote at run time is
// read back from the module's own file (relocated to the live base), and a vtable lookup no longer ends at the foreign
// pointer. The "module" is this test executable (relocated by ASLR: the relocation path runs); the "vtable" a const
// table of function pointers in its .rdata, one entry rewritten in memory to a pointer outside every module (as FLUF's
// VTableHook does with a pointer into FLUF.dll).
#include "hook.h"

#include "test.h"

namespace
{
__declspec(noinline) int OrigA(int x) { return x + 1; }
__declspec(noinline) int OrigB(int x) { return x + 2; }
__declspec(noinline) int OrigC(int x) { return x + 3; }
typedef int (*Fn)(int);
extern const Fn kTable[4];
const Fn kTable[4] = {OrigA, OrigB, OrigC, nullptr};
} // namespace

void Test_orig_image()
{
    HMODULE self = GetModuleHandleA(nullptr);
    void* const* vt = reinterpret_cast<void* const*>(kTable);
    const uintptr_t slot1 = reinterpret_cast<uintptr_t>(&kTable[1]);

    // untouched: live and original agree
    CHECK(hk::VtableSlot(self, vt, reinterpret_cast<void*>(OrigC)) == 2);
    uintptr_t orig = 0;
    CHECK(hk::OriginalPointer(slot1, &orig) && orig == reinterpret_cast<uintptr_t>(OrigB));

    // another "mod" rewrites entry 1 to code outside every module
    void* foreign = hk::AllocExec(16);
    CHECK(foreign != nullptr);
    CHECK(hk::Write(slot1, &foreign, sizeof(foreign)));
    CHECK(*reinterpret_cast<void* const volatile*>(slot1) == foreign); // (volatile: the const table is not folded)
    // the live scan alone would stop at entry 1; the lookup finds OrigC by the file's entries
    CHECK(hk::VtableSlot(self, vt, reinterpret_cast<void*>(OrigC)) == 2);
    CHECK(hk::VtableSlot(self, vt, reinterpret_cast<void*>(OrigB)) == 1);
    // the original pointer comes from the file, relocated: the function's live address
    orig = 0;
    CHECK(hk::OriginalPointer(slot1, &orig) && orig == reinterpret_cast<uintptr_t>(OrigB));
    // a method that is in no entry stays unknown; outside any module = refused
    CHECK(hk::VtableSlot(self, vt, reinterpret_cast<void*>(Test_orig_image)) == -1);
    CHECK(hk::OriginalAt(reinterpret_cast<uintptr_t>(foreign), 4) == nullptr);
    uintptr_t none = 0;
    CHECK(!hk::OriginalPointer(reinterpret_cast<uintptr_t>(foreign), &none));

    // put it back (the table is the test's own)
    void* b = reinterpret_cast<void*>(OrigB);
    CHECK(hk::Write(slot1, &b, sizeof(b)));
    CHECK(OrigB(1) == 3 && OrigA(1) == 2 && OrigC(1) == 4);
}
