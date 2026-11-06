// String helpers shared by every module part. Header-only on purpose: the logic and each host include it in their own
// scope, so inline functions here cost no extra translation unit and no link step. ASCII semantics, as the game's
// data and the harness commands are: Lower() maps each byte through tolower() as an unsigned char.
//   Trim(s, ws)       strips the characters of `ws` from both ends. The default set " \t\r" is the ini files' (a
//                     line's '\n' is already gone after getline); callers whose text may still carry line ends pass
//                     " \t\r\n" (core\game\ai.cpp). The sets are deliberately not unified: each call site keeps its own.
//   StripComment(s)   cuts a line at its ';' (the ini comment character); Trim(StripComment(line)) is the ini
//                     reader's line normalisation (core\game\ini.cpp, space.cpp).
#pragma once
#include "common.h"

#include <string>

namespace str
{
inline std::string Trim(std::string s, const char* ws = " \t\r")
{
    s.erase(0, s.find_first_not_of(ws));
    s.erase(s.find_last_not_of(ws) + 1);
    return s;
}

inline std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

inline std::string StripComment(std::string s)
{
    if (const size_t semi = s.find(';'); semi != std::string::npos)
        s.erase(semi);
    return s;
}

// UTF-8 <-> UTF-16 (the text the game and Windows draw). Length-based: an embedded NUL is kept. The one conversion
// of every module part (text.cpp, the subtitles' data.cpp and uikit.cpp each had their own).
inline std::wstring Widen(const char* utf8, size_t len)
{
    if (!utf8 || !len)
        return L"";
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len), nullptr, 0);
    std::wstring w(n > 0 ? n : 0, L'\0');
    if (n > 0)
        MultiByteToWideChar(CP_UTF8, 0, utf8, static_cast<int>(len), &w[0], n);
    return w;
}
inline std::wstring Widen(const std::string& s) { return Widen(s.data(), s.size()); }
inline std::wstring Widen(const char* s) { return s ? Widen(s, strlen(s)) : L""; }

inline std::string Narrow(const std::wstring& w)
{
    if (w.empty())
        return "";
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n > 0 ? n : 0, '\0');
    if (n > 0)
        WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}

// A name in capitals, as the game's own all-caps labels read: towupper is ASCII-only under the C runtime's "C" locale
// (GRAFENWoHR with a small o-umlaut). The invariant locale keeps the result the same on every machine (no Turkish dotted I);
// the German sharp s has no capital in the fonts, so it spells SS (GROSSGLOCKNER). ASCII never leaves this loop.
inline std::wstring Upper(std::wstring s)
{
    bool ascii = true;
    for (wchar_t& c : s)
        if (c >= L'a' && c <= L'z')
            c = static_cast<wchar_t>(c - L'a' + L'A');
        else if (c >= 0x80)
            ascii = false;
    if (ascii)
        return s;
    for (size_t i = s.find(static_cast<wchar_t>(0xDF)); i != std::wstring::npos; i = s.find(static_cast<wchar_t>(0xDF), i))
        s.replace(i, 1, L"SS");
    const int n = static_cast<int>(s.size());
    std::wstring up(s.size(), L'\0');
    return LCMapStringW(LOCALE_INVARIANT, LCMAP_UPPERCASE, s.data(), n, &up[0], n) == n ? up : s;
}

// The first letter in capitals (a sentence's start); a letter with no one-character capital stays.
inline void Capitalize(std::wstring& s)
{
    if (s.empty())
        return;
    const std::wstring first = Upper(s.substr(0, 1));
    if (first.size() == 1)
        s[0] = first[0];
}

// Wide text against UTF-8 text, exact: every character counts, not only ASCII.
inline bool SameText(const std::wstring& w, const std::string& utf8) { return Narrow(w) == utf8; }
} // namespace str
