// core\str.h: the shared string helpers (since 29 Oct 2026 also the one UTF-8 <-> UTF-16 conversion every module part
// uses - text.cpp's Utf8, the subtitles' data.cpp and uikit.cpp had their own copies).
#include "str.h"

#include "game/glyphset.h"
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

    // capitals: the game's names past ASCII, each by its UTF-8 bytes
    CHECK(str::Upper(L"Fort Bush 7") == L"FORT BUSH 7");
    CHECK(str::Upper(str::Widen("Grafenw\xC3\xB6hr")) == str::Widen("GRAFENW\xC3\x96HR"));
    CHECK(str::Upper(str::Widen("Cura\xC3\xA7""ao")) == str::Widen("CURA\xC3\x87" "AO"));
    CHECK(str::Upper(str::Widen("Pequ\xC3\xB1" "a")) == str::Widen("PEQU\xC3\x91" "A"));
    CHECK(str::Upper(str::Widen("Gro\xC3\x9Fglockner")) == L"GROSSGLOCKNER");
    CHECK(str::Upper(str::Widen("Death\xE2\x80\x99s Hand")) == str::Widen("DEATH\xE2\x80\x99S HAND"));
    CHECK(str::Upper(str::Widen("\xC5\x93 \xC5\xA1 \xC5\xBE \xC3\xBF")) == str::Widen("\xC5\x92 \xC5\xA0 \xC5\xBD \xC5\xB8"));
    CHECK(str::Upper(L"").empty());
    // what comes out has its glyph: the capital of every character of the fonts' set is in the set
    for (const wchar_t c : glyphset::Base())
        for (const wchar_t up : str::Upper(std::wstring(1, c)))
            CHECK(glyphset::Base().find(up) != std::wstring::npos);

    std::wstring sentence = str::Widen("\xC3\xBC" "ber");
    str::Capitalize(sentence);
    CHECK(sentence == str::Widen("\xC3\x9C" "ber"));
    sentence = str::Widen("\xC3\x9F");
    str::Capitalize(sentence);
    CHECK(sentence == str::Widen("\xC3\x9F"));

    // a rep name against its id's text: the names of M07, M08 and M10 (the old compare folded them to '?' and never matched)
    CHECK(str::SameText(str::Widen("Cruiser RNC K\xC3\xB6ln"), "Cruiser RNC K\xC3\xB6ln"));
    CHECK(str::SameText(str::Widen("Tekagi\xE2\x80\x99s Transport"), "Tekagi\xE2\x80\x99s Transport"));
    CHECK(!str::SameText(str::Widen("Cruiser RNC K\xC3\xB6ln"), "Cruiser RNC K?ln"));
    CHECK(!str::SameText(L"Cruiser RNC Bonn", "Cruiser RNC K\xC3\xB6ln"));
}
