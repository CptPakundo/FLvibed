// The crash recorder's pure half (core\host\crashlog_rules.h): the fixed-buffer text, the exceptions left to the game, and the
// test that a stack value returns behind a call.
#include "../core/host/crashlog_rules.h"

#include <cstring>

#include "test.h"

void Test_crashlog()
{
    using namespace crashrules;
    char b[64];
    Text t(b, sizeof(b));
    t.Put("code ").Hex(0xC0000005u).Put(" at +").Hex(0x1A2Bu, 4).Put(" pid ").Dec(39120).Put(" ").Dec(0).Put(" t ").Dec2(7);
    CHECK(strcmp(b, "code 0xC0000005 at +0x1A2B pid 39120 0 t 07") == 0);
    CHECK(t.len == strlen(b));
    // never past the end, always terminated
    char s[8];
    Text u(s, sizeof(s));
    u.Put("0123456789").Hex(0xFFFFFFFFu);
    CHECK(u.len == 7 && strcmp(s, "0123456") == 0);
    Text z(s, 1);
    z.Put("x");
    CHECK(z.len == 0 && s[0] == '\0');
    CHECK(strcmp(Text(b, sizeof(b)).Hex(0xABu, 0).buf, "0x000000AB") == 0); // an out-of-range width: all 8 digits

    // the game's filter passes these two on, so does the recorder
    CHECK(PassedOn(0x80000003u) && PassedOn(0x80000004u) && !PassedOn(0xC0000005u) && !PassedOn(0xC00000FDu));
    // first chance: only the fatal codes; never a C++ exception, an access violation (the game's SEH guards catch some),
    // a breakpoint or a debugger's message
    CHECK(FatalFirstChance(0xC0000029u) && FatalFirstChance(0xC00000FDu) && FatalFirstChance(0xC0000374u) && FatalFirstChance(0xC0000409u));
    CHECK(!FatalFirstChance(0xE06D7363u) && !FatalFirstChance(0xC0000005u) && !FatalFirstChance(0x80000003u) && !FatalFirstChance(0x406D1388u));
    CHECK(strcmp(AccessKind(0), "read") == 0 && strcmp(AccessKind(1), "write") == 0 && strcmp(AccessKind(8), "execute") == 0);

    // return addresses: the 7 bytes before the value, call forms of every length
    auto ret = [](std::initializer_list<int> tail) {
        unsigned char before[7] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
        size_t i = 7 - tail.size();
        for (int v : tail)
            before[i++] = static_cast<unsigned char>(v);
        return LooksLikeReturn(before, 7);
    };
    CHECK(ret({0xE8, 0x10, 0x20, 0x30, 0x40}));              // call rel32
    CHECK(ret({0xFF, 0xD0}) && ret({0xFF, 0xD7}));           // call eax / edi
    CHECK(ret({0xFF, 0x10}) && ret({0xFF, 0x16}));           // call [eax] / [esi]
    CHECK(ret({0xFF, 0x50, 0x08}));                          // call [eax+8]
    CHECK(ret({0xFF, 0x14, 0x85}));                          // call [sib]
    CHECK(ret({0xFF, 0x54, 0x24, 0x08}));                    // call [esp+8]
    CHECK(ret({0xFF, 0x15, 0x00, 0x10, 0x60, 0x00}));        // call [abs32] (an import)
    CHECK(ret({0xFF, 0x90, 0x00, 0x01, 0x00, 0x00}));        // call [eax+0x100]
    CHECK(ret({0xFF, 0x94, 0x24, 0x00, 0x01, 0x00, 0x00}));  // call [esp+0x100]
    CHECK(!ret({}));                                          // no call
    CHECK(!ret({0xFF, 0xE0}));                                // jmp eax (FF /4)
    CHECK(!ret({0xFF, 0x30}));                                // push [eax] (FF /6)
    CHECK(!ret({0xFF, 0x15}));                                // call [abs32] needs its 4 bytes after
    CHECK(!ret({0xFF, 0x14}));                                // a SIB form with its SIB byte missing
    unsigned char shortRun[2] = {0xFF, 0xD0};
    CHECK(LooksLikeReturn(shortRun, 2) && !LooksLikeReturn(shortRun, 1)); // fewer readable bytes: only those count
}
