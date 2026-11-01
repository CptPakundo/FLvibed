// core\str.h: the shared string helpers (since 29 Oct 2026 also the one UTF-8 <-> UTF-16 conversion every module part
// uses - text.cpp's Utf8, the subtitles' data.cpp and uikit.cpp had their own copies).
#include "str.h"

#include "test.h"

void Test_str()
{
    CHECK(str::Trim("  a b \t\r") == "a b");
    CHECK(str::Trim("x\n", " \t\r\n") == "x");
    CHECK(str::Lower("Li01_Trade_Lane") == "li01_trade_lane");
    CHECK(str::StripComment("key = v ; note") == "key = v ");
    // UTF-8 round trips: ASCII, Latin-1, the speaker separator (U+00B7), an em dash, a non-BMP character
    const char* samples[] = {"Juni", "Tekagi's garden", "Fort Bush \xC2\xB7 Police", "Trent \xE2\x80\x94 run",
                             "\xF0\x9F\x8C\x9F"};
    for (const char* s : samples)
        CHECK(str::Narrow(str::Widen(s)) == s);
    CHECK(str::Widen("\xC2\xB7") == L"\x00B7");
    CHECK(str::Widen("\xF0\x9F\x8C\x9F").size() == 2); // a surrogate pair
    CHECK(str::Widen(static_cast<const char*>(nullptr)).empty() && str::Widen("").empty() && str::Narrow(L"").empty());
    // length-based: an embedded NUL is kept (the old uikit copy stopped at it)
    const std::string withNul("a\0b", 3);
    CHECK(str::Widen(withNul).size() == 3 && str::Narrow(str::Widen(withNul)) == withNul);
}
