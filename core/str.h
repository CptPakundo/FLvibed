// String helpers shared by every module part. Header-only on purpose: each part compiles core into its own namespace
// wrapper, so inline functions here cost no extra translation unit and no link step. ASCII semantics, as the game's
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
} // namespace str
