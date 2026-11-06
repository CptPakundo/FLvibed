// core\game\glyphset.h (pure): GitHub issue 37, "DEATH?S HAND MK II" in the weapon group window. The game's apostrophe is
// U+2019 and the row kit's atlas was ASCII and five more, so render's lookup fell to '?'. Checks that every character the
// game's string tables use (HDE 0.7.1, listed from the resource DLLs on 6 Nov 2026) resolves to itself or its stand-in,
// never to '?', and, with the install's font, that whatever Agency FB lacks of the set has a stand-in it holds.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "test.h"

#include "game/glyphset.h"

#include <windows.h>

static bool InBase(wchar_t c) { return glyphset::Base().find(c) != std::wstring::npos; }

static void FontCovers()
{
    if (g_test.install.empty())
    {
        ++g_test.skipped;
        return;
    }
    const std::string f = g_test.install + "\\DATA\\FONTS\\AGENCYB.TTF";
    const std::wstring wf(f.begin(), f.end());
    AddFontResourceExW(wf.c_str(), FR_PRIVATE, nullptr);
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT hf = CreateFontW(-32, 0, 0, 0, 700, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Agency FB");
    HGDIOBJ old = SelectObject(dc, hf);
    wchar_t got[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, got);
    if (_wcsicmp(got, L"Agency FB") != 0)
        ++g_test.skipped;
    else
    {
        const auto holds = [&](wchar_t c)
        {
            WORD index = 0xFFFF;
            GetGlyphIndicesW(dc, &c, 1, &index, GGI_MARK_NONEXISTING_GLYPHS);
            return index != 0xFFFF;
        };
        CHECK(holds(0x2019)); // the issue's character: the face has it, the atlas lacked it
        for (const wchar_t c : glyphset::Base())
            if (!holds(c))
                CHECK(glyphset::Nearest(c) && holds(glyphset::Nearest(c)));
    }
    SelectObject(dc, old);
    DeleteObject(hf);
    DeleteDC(dc);
    RemoveFontResourceExW(wf.c_str(), FR_PRIVATE, nullptr);
}

void Test_glyphset()
{
    // the failing character, in the old atlas (ASCII + degree, middle dot, dashes, ellipsis) and in the set
    const std::wstring old = [&]
    {
        std::wstring s;
        for (wchar_t c = 32; c < 127; ++c)
            s += c;
        return s + static_cast<wchar_t>(0xB0) + static_cast<wchar_t>(0xB7) + static_cast<wchar_t>(0x2013) +
               static_cast<wchar_t>(0x2014) + static_cast<wchar_t>(0x2026);
    }();
    const auto inOld = [&](wchar_t c) { return old.find(c) != std::wstring::npos; };
    CHECK(glyphset::Resolve(0x2019, inOld) == L'\'');
    CHECK(glyphset::Resolve(0x2019, InBase) == 0x2019);

    // every non-ASCII character of the game's string tables
    for (const int code : {0x00B0, 0x00B2, 0x00C7, 0x00DC, 0x00DF, 0x00E4, 0x00E7, 0x00E9, 0x00F1, 0x00F6, 0x00FC, 0x2007, 0x200A,
                           0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x2074})
    {
        const wchar_t c = static_cast<wchar_t>(code);
        CHECK(glyphset::Resolve(c, InBase) != L'?');
        CHECK(InBase(c) || glyphset::Nearest(c));
    }

    // the table: the mark goes, the letter stays in its place (a mod's Polish or Czech name on a face without them)
    CHECK(glyphset::Nearest(0x0100) == L'A');
    CHECK(glyphset::Nearest(0x010D) == L'c');
    CHECK(glyphset::Nearest(0x0141) == L'L');
    CHECK(glyphset::Nearest(0x0144) == L'n');
    CHECK(glyphset::Nearest(0x0160) == L'S');
    CHECK(glyphset::Nearest(0x017E) == L'z');
    CHECK(glyphset::Nearest(0x2011) == L'-');
    CHECK(glyphset::Nearest(0x201D) == L'"');
    CHECK(glyphset::Nearest(0x2076) == L'6');
    for (wchar_t c = 0x0100; c <= 0x017F; ++c)
        CHECK(InBase(glyphset::Nearest(c)));

    // another script has no stand-in: '?', and nothing from a font without one
    CHECK(glyphset::Nearest(0x0416) == 0);
    CHECK(glyphset::Resolve(0x0416, InBase) == L'?');
    CHECK(glyphset::Resolve(0x0416, [](wchar_t) { return false; }) == 0);
    FontCovers();
}
