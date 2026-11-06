// The characters FLvibed's fonts hold, and the one table of what stands in for a character a font cannot show. Pure: no
// game, no device (tests\glyphset_test.cpp). Every atlas of render::BuildFont starts from Base(); render::GlyphFor walks
// Resolve() for each character it draws, and adds what was missing to render::Unmet() for the next build.
//
// Why a wide set: the game's names are not ASCII. The string tables of HDE 0.7.1 hold U+2019 in 1343 strings (the guns
// Death's Hand, Gaia's Savior, Thor's Hammer), then the dashes, the double quotes, the bullet, the ellipsis, and German,
// French and Spanish letters. Each face differs: Agency FB has Latin-1 and those punctuation marks but only ten letters
// of Latin Extended-A, no arrows, no superscripts; Arial Unicode MS has them all. GDI draws some of what a face lacks
// from another font (the arrows, CJK) and the face's empty box for the rest: render::BuildFont leaves the box out.
#pragma once

#include <string>

namespace glyphset
{
// ASCII, Latin-1 Supplement, General Punctuation's dashes, quotes, bullet and ellipsis, the arrows, the minus and the
// letters of Windows-1252 past Latin-1.
inline const std::wstring& Base()
{
    static const std::wstring set = []
    {
        std::wstring c;
        for (wchar_t ch = 32; ch < 127; ++ch)
            c += ch;
        for (wchar_t ch = 0xA0; ch <= 0xFF; ++ch)
            c += ch;
        for (wchar_t ch = 0x2010; ch <= 0x2027; ++ch)
            c += ch;
        for (wchar_t ch = 0x2190; ch <= 0x2193; ++ch)
            c += ch;
        for (const int ch : {0x2212, 0x0152, 0x0153, 0x0160, 0x0161, 0x0178, 0x017D, 0x017E})
            c += static_cast<wchar_t>(ch);
        return c;
    }();
    return set;
}

// The nearest character of ASCII / Latin-1 to `c`; 0 when there is none (another script: its own letters or nothing).
inline wchar_t Nearest(wchar_t c)
{
    // U+0100 - U+017F, Latin Extended-A: each letter without its mark
    static const char kLatinA[] = "AaAaAaCcCcCcCcDdDdEeEeEeEeEeGgGgGgGgHhHhIiIiIiIiIiIiJjKkkLlLlLlLlLlNnNnNnnNnOoOoOoOoRrRrRrSsSsSsSsTtTtTtUuUuUuUuUuUuWwYyYZzZzZzs";
    static_assert(sizeof(kLatinA) == 0x80 + 1, "one letter for each of U+0100 - U+017F");
    static const struct
    {
        wchar_t from, to, sub;
    } kNear[] = {
        {0x2000, 0x200A, L' '},    // the spaces
        {0x202F, 0x202F, L' '},    //
        {0x205F, 0x205F, L' '},    //
        {0x3000, 0x3000, L' '},    //
        {0x02BC, 0x02BC, L'\''},   // the modifier apostrophe
        {0x2018, 0x201B, L'\''},   // the single quotes
        {0x2032, 0x2032, L'\''},   // prime
        {0x201C, 0x201F, L'"'},    // the double quotes
        {0x2033, 0x2033, L'"'},    // double prime
        {0x2010, 0x2015, L'-'},    // the hyphens and dashes
        {0x2212, 0x2212, L'-'},    // minus
        {0x2016, 0x2016, L'|'},    // double vertical line
        {0x2017, 0x2017, L'_'},    // double low line
        {0x2020, 0x2021, L'+'},    // the daggers
        {0x2022, 0x2023, 0x00B7},  // the bullets: the middle dot
        {0x2027, 0x2027, 0x00B7},  //
        {0x2024, 0x2026, L'.'},    // the leaders, the ellipsis
        {0x2190, 0x2190, L'<'},    // the arrows
        {0x2191, 0x2191, L'^'},    //
        {0x2192, 0x2192, L'>'},    //
        {0x2193, 0x2193, L'v'},    //
        {0x2070, 0x2070, L'0'},    // the superscripts
    };
    if (c >= 0x0100 && c <= 0x017F)
        return static_cast<wchar_t>(kLatinA[c - 0x0100]);
    if (c >= 0x2074 && c <= 0x2079)
        return static_cast<wchar_t>(L'4' + (c - 0x2074));
    for (const auto& n : kNear)
        if (c >= n.from && c <= n.to)
            return n.sub;
    return 0;
}

// What to draw for `c` from a font that holds what has(ch) says: `c`, else its nearest, else '?', else 0 (nothing).
template <typename Has> inline wchar_t Resolve(wchar_t c, const Has& has)
{
    if (has(c))
        return c;
    if (const wchar_t n = Nearest(c); n && has(n))
        return n;
    return has(L'?') ? L'?' : 0;
}
} // namespace glyphset
