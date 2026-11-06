// Movie lines' word timings (pure; tests\movie_words_test.cpp). The review's aligner writes the intro lines into
// word_times.tsv like the dialogue (subtitles\review\FLSubtitles\aligner\align_words.py, movie mode): id = the movie,
// voice "movie", the line's text hash (FNV-1a of the UTF-8 text field), end_ms and starts_ms on the MOVIE clock
// (ms from the movie's start; -1 = a word the aligner could not place). movie.cpp finds a line's row by movie + text
// hash (a changed text keeps the estimate, as data.cpp does for dialogue) and hands display::Show the starts from the
// line's own start.
#pragma once
// (atoi / iswspace: core\pre.h in the module build; the test includes <cstdlib> / <cwctype> first)
#include <string>
#include <vector>

namespace moviewords
{
constexpr const char* kVoice = "movie";

// FNV-1a of the UTF-8 text, the hash align_words.py writes (text_fnv).
inline unsigned TextFnv(const std::string& utf8)
{
    unsigned h = 0x811C9DC5u;
    for (const char c : utf8)
        h = (h ^ static_cast<unsigned char>(c)) * 0x01000193u;
    return h;
}

// Whitespace-separated words, as the aligner counts them.
inline size_t WordCount(const std::wstring& text)
{
    size_t n = 0;
    for (size_t i = 0; i < text.size(); ++i)
        n += !iswspace(text[i]) && (i == 0 || iswspace(text[i - 1])) ? 1 : 0;
    return n;
}

// starts_ms ("5700,5810,-1,...", movie clock) -> each word's start in seconds from the line's start (lineStart, s);
// -1 stays unknown (< 0: the page keeps its estimate). A word placed before the line's start counts from 0. False (no
// timings, the estimate) when the count is not `words` or no word is placed.
inline bool Relative(const std::string& field, float lineStart, size_t words, std::vector<float>& out)
{
    out.clear();
    size_t placed = 0;
    for (size_t a = 0; a <= field.size();)
    {
        size_t b = field.find(',', a);
        if (b == std::string::npos)
            b = field.size();
        if (b == a)
            return out.clear(), false;
        const int ms = atoi(field.substr(a, b - a).c_str());
        if (ms < 0)
            out.push_back(-1.0f);
        else
        {
            const float t = ms / 1000.0f - lineStart;
            out.push_back(t < 0 ? 0.0f : t);
            ++placed;
        }
        a = b + 1;
    }
    if (out.size() != words || !placed)
        return out.clear(), false;
    return true;
}
} // namespace moviewords
