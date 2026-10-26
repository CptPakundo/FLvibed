// core\game\flhash.cpp against Common.dll's CreateID as tools\flhash.ps1 computes it (reference values taken from that
// tool on 24 Oct 2026; the tool's values are the ones the voice banks and the harness are matched with).
#include "test.h"

#include "game/flhash.h"

void Test_flhash()
{
    struct Ref
    {
        const char* name;
        u32 id;
    };
    const Ref refs[] = {
        {"li01", 0x83A78E4Fu},
        {"Li01_01_Base", 0xA7303E0Du},
        {"DX_M10_0460B_BOTZLER", 0x899657CCu},
        {"Li01_02_dock_ring", 0xA254A3CDu},
        {"prisonship", 0x93EA4946u},
        {"St01", 0xB7FFDC83u},
        {"Br04_01", 0xABE19941u},
        {"a", 0xA00C1407u},
        {"", 0x80000000u},
    };
    for (const Ref& r : refs)
    {
        const u32 got = flhash::CreateId(r.name);
        if (got != r.id)
            fprintf(stderr, "  flhash(%s) = 0x%08X, expected 0x%08X\n", r.name, got, r.id);
        CHECK(got == r.id);
    }
    // case-insensitive, std::string overload, the top bit always set
    CHECK(flhash::CreateId("LI01_02_DOCK_RING") == 0xA254A3CDu);
    CHECK(flhash::CreateId(std::string("li01")) == 0x83A78E4Fu);
    CHECK((flhash::CreateId("anything") & 0x80000000u) != 0);
    CHECK(flhash::CreateId("li01") != flhash::CreateId("li02"));
}
