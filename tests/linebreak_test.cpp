// subtitles\src\impl\linebreak.cpp (the subtitles' row and page breaking, moved out of display.cpp 29 Oct 2026; it had no
// tests: SWEEP_2026-10-29 #10). A monospace width (10 px a character) makes the expectations exact. Then every shipped
// subtitle line (subtitles\data\subtitles.tsv) is laid out at three widths: the pages cover the line's words in order,
// each page fits two rows, no cut falls inside the name prefix.
#include "test.h"

#include "../subtitles/src/impl/linebreak.h"

#include <windows.h>

using linebreak::Fit;

static int Mono(const std::wstring& s, const void*) { return static_cast<int>(s.size()) * 10; }
static Fit F(int maxChars, size_t nameWords = 0) { return {Mono, nullptr, maxChars * 10, nameWords}; }
static std::vector<std::wstring> W(const wchar_t* s) { return linebreak::SplitWords(s); }

static std::wstring Widen(const std::string& s)
{
    std::wstring w(MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), static_cast<int>(w.size()));
    return w;
}

void Test_linebreak()
{
    // words: runs of spaces collapse; Join puts single spaces back
    CHECK(W(L"  a  bb c ").size() == 3);
    CHECK(linebreak::Join(W(L"a  bb c"), 0, 3) == L"a bb c");

    // one row when it fits; no break at all when it fits one row is the caller's choice (FitsPage)
    CHECK(linebreak::FitsPage(F(40), W(L"Short line."), 0, 2));

    // a sentence end wins the break: "Go now." / "The base is under attack."
    {
        const auto w = W(L"Go now. The base is under attack.");
        CHECK(linebreak::BestBreak(F(26), w, 0, w.size()) == 2);
    }
    // never after a weak word, never inside a capitalised name ("Planet / Manhattan")
    {
        const auto w = W(L"We fly to Planet Manhattan and land there");
        const size_t k = linebreak::BestBreak(F(28), w, 0, w.size());
        CHECK(k != 0);
        CHECK(!linebreak::WeakWord(w[k - 1]));
        CHECK(!(w[k - 1] == L"Planet" && w[k] == L"Manhattan"));
    }
    // the bottom-heavy pyramid: with no clause to prefer, the top row is not the longer one
    {
        const auto w = W(L"one two three four five six seven eight");
        const size_t k = linebreak::BestBreak(F(30), w, 0, w.size());
        CHECK(k != 0 && Mono(linebreak::Join(w, 0, k), nullptr) <= Mono(linebreak::Join(w, k, w.size()), nullptr));
    }
    // the name prefix ("Juni:" = 1 word): no cut inside it; right after it only when nothing else fits
    {
        const auto w = W(L"Agent Juni: Trent, follow me to the station now.");
        const size_t k = linebreak::BestBreak(F(30, 2), w, 0, w.size());
        CHECK(k > 2);
        const auto narrow = W(L"Agent Juni: Go.");
        CHECK(linebreak::BestBreak(F(11, 2), narrow, 0, narrow.size()) == 2);
    }
    // pages: two short sentences share a page; a long one is split evenly at a clause end
    {
        const auto w = W(L"Hello. Come in.");
        const auto s = linebreak::PageSpans(F(40), w);
        CHECK(s.size() == 1 && s[0].first == 0 && s[0].second == w.size());
        const auto l = W(L"We will hold the line here at the gate, and the convoy will pass behind us while the "
                         L"fighters keep the pirates busy on the far side of the field.");
        const auto p = linebreak::PageSpans(F(30), l);
        CHECK(p.size() >= 2);
        size_t at = 0;
        for (const auto& sp : p)
        {
            CHECK(sp.first == at && sp.second > sp.first);
            CHECK(linebreak::FitsPage(F(30), l, sp.first, sp.second));
            at = sp.second;
        }
        CHECK(at == l.size());
    }

    // every shipped line at three widths: full in-order coverage, every page fits (unless a single word is wider than
    // the row), no cut inside the name prefix
    if (!NeedInstall("the shipped subtitle lines (repository data)"))
        return;
    const std::vector<std::string> rows = ReadLines(g_test.install + "\\..\\..\\subtitles\\data\\subtitles.tsv");
    CHECK(rows.size() > 1000);
    int lines = 0, pages = 0, bad = 0;
    for (size_t r = 1; r < rows.size(); ++r)
    {
        // columns: id, voice, kind, source, sec, speaker, text
        std::vector<std::string> c;
        size_t a = 0;
        for (size_t b; (b = rows[r].find('\t', a)) != std::string::npos; a = b + 1)
            c.push_back(rows[r].substr(a, b - a));
        c.push_back(rows[r].substr(a));
        if (c.size() < 7 || c[6].empty())
            continue;
        ++lines;
        const std::wstring prefix = c[5].empty() ? L"" : Widen(c[5]) + L": ";
        const auto w = linebreak::SplitWords(prefix + Widen(c[6]));
        const size_t nameWords = prefix.empty() ? 0 : linebreak::SplitWords(prefix).size();
        for (const int chars : {32, 48, 70})
        {
            const Fit f = F(chars, nameWords);
            const auto spans = linebreak::PageSpans(f, w);
            size_t at = 0;
            bool ok = !spans.empty();
            for (const auto& sp : spans)
            {
                ok = ok && sp.first == at && sp.second > sp.first;
                ok = ok && (sp.second - sp.first == 1 || linebreak::FitsPage(f, w, sp.first, sp.second));
                ok = ok && !(sp.second < nameWords); // a page never ends inside the name
                at = sp.second;
            }
            ok = ok && at == w.size();
            pages += static_cast<int>(spans.size());
            if (!ok && ++bad <= 5)
                fprintf(stderr, "  linebreak: line %s at %d chars: bad pages\n", c[0].c_str(), chars);
        }
    }
    CHECK(lines > 1000);
    CHECK(bad == 0);
    if (g_test.verbose)
        printf("  linebreak: %d shipped lines laid out at 3 widths: %d pages, %d bad\n", lines, pages, bad);
}
