// The subtitles' line breaking (moved out of display.cpp on 29 Oct 2026 unchanged, so the unit tests can hold it:
// tests\linebreak_test.cpp): words, the best two-row break, pages that follow sentences and the even split of a
// sentence too long for one page. The text width is a callback (display.cpp: the subtitle font); `nameWords` = the
// words of an inline "Name:" prefix at the block's start (0 none), inside which no row or page is cut.
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace linebreak
{
struct Fit
{
    int (*width)(const std::wstring& text, const void* ctx); // pixels
    const void* ctx;
    int maxW;         // a row's width
    size_t nameWords; // the inline name prefix's words (0 none)
};

std::vector<std::wstring> SplitWords(const std::wstring& s);
std::wstring Join(const std::vector<std::wstring>& w, size_t a, size_t b);
// An article / preposition / pronoun a row should not end on.
bool WeakWord(const std::wstring& w);
// The word ends a sentence (. ? ! ...), or with !sentenceOnly a clause (, ; : en dash) too.
bool EndsClause(const std::wstring& word, bool sentenceOnly);
// Best two-row split of words[a,b): the index of the second row's first word, 0 when no split fits. Both rows fit, the
// top row a little shorter (bottom-heavy pyramid), no row ending in a weak word, a clause / sentence end preferred,
// never inside the name, never between two capitalised words of a name.
size_t BestBreak(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b);
// Words [a,b) fit on one page: one row, or two rows with a valid break.
bool FitsPage(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b);
// Splits words [a,b) (too long for one page) into the fewest pages of about equal length, at clause ends where possible.
void SplitEven(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b,
               std::vector<std::pair<size_t, size_t>>& out);
// A block's pages as word spans: whole sentences share a page while they fit in two rows; a sentence too long for one
// page is cut into even parts at clause ends.
std::vector<std::pair<size_t, size_t>> PageSpans(const Fit& f, const std::vector<std::wstring>& w);
} // namespace linebreak
