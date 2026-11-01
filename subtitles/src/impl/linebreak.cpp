// subtitles\src\impl\linebreak.h - the bodies as they were in display.cpp (29 Oct 2026), the font width and the name
// prefix now parameters (Fit) instead of display.cpp's g_font / s_nameWords.
#include "linebreak.h"

#include <algorithm>
#include <cmath>

namespace linebreak
{
static bool InName(const Fit& f, size_t k) { return k <= f.nameWords; }

bool WeakWord(const std::wstring& w)
{
    static const wchar_t* weak[] = {L"a",     L"an",   L"the", L"to",   L"of", L"in",   L"on",  L"at",  L"for",
                                    L"and",   L"with", L"by",  L"from", L"my", L"your", L"our", L"his", L"her",
                                    L"their", L"its",  L"is",  L"be",   L"or", L"but",  L"as",  L"if",  L"no"};
    std::wstring l;
    for (wchar_t c : w)
        l += static_cast<wchar_t>(towlower(c));
    for (const wchar_t* x : weak)
        if (l == x)
            return true;
    return false;
}

std::vector<std::wstring> SplitWords(const std::wstring& s)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s)
    {
        if (c == L' ')
        {
            if (!cur.empty())
                out.push_back(cur), cur.clear();
        }
        else
            cur += c;
    }
    if (!cur.empty())
        out.push_back(cur);
    return out;
}

std::wstring Join(const std::vector<std::wstring>& w, size_t a, size_t b)
{
    std::wstring s;
    for (size_t i = a; i < b; ++i)
        s += (i > a ? L" " : L"") + w[i];
    return s;
}

size_t BestBreak(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b)
{
    const int maxW = f.maxW;
    size_t best = 0;
    double bestCost = 1e30;
    for (size_t k = a + 1; k < b; ++k)
    {
        // never inside the name; right after it ("Name:" alone on the first row) only when nothing else fits (user, 22
        // Oct 2026: a long "Name · Faction" with a short line had no valid break, and the page was cut after the
        // name's first word - "Agent" alone, the rest uncoloured)
        if (InName(f, k) && k != f.nameWords)
            continue;
        const int w1 = f.width(Join(w, a, k), f.ctx), w2 = f.width(Join(w, k, b), f.ctx);
        if (w1 > maxW || w2 > maxW)
            continue;
        double cost = std::abs(w1 - w2) + (w1 > w2 ? (w1 - w2) * 0.5 : 0);
        if (k == f.nameWords)
            cost += maxW * 4.0;
        if (WeakWord(w[k - 1]))
            cost += maxW * 0.5;
        const wchar_t last = w[k - 1].back();
        if (last == L'.' || last == L'?' || last == L'!' || last == L'\x2026')
            cost -= maxW * 0.7; // a sentence per row reads best
        else if (last == L',' || last == L';' || last == L':' || last == L'\x2013')
            cost -= maxW * 0.3;
        // never split a name ("Fort / Bush", "Planet / Manhattan")
        if (iswupper(w[k - 1][0]) && iswupper(w[k][0]) && iswalpha(last))
            cost += maxW * 0.6;
        if (cost < bestCost)
            bestCost = cost, best = k;
    }
    return best;
}

bool EndsClause(const std::wstring& word, bool sentenceOnly)
{
    const wchar_t c = word.back();
    if (c == L'.' || c == L'?' || c == L'!' || c == L'\x2026')
        return true;
    return !sentenceOnly && (c == L',' || c == L';' || c == L':' || c == L'\x2013');
}

bool FitsPage(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b)
{
    return f.width(Join(w, a, b), f.ctx) <= f.maxW || BestBreak(f, w, a, b) != 0;
}

void SplitEven(const Fit& f, const std::vector<std::wstring>& w, size_t a, size_t b,
               std::vector<std::pair<size_t, size_t>>& out)
{
    const int maxW = f.maxW;
    while (a < b)
    {
        if (FitsPage(f, w, a, b))
        {
            out.push_back({a, b});
            return;
        }
        const int total = f.width(Join(w, a, b), f.ctx);
        const int pages = std::max(2, (total + 2 * maxW * 9 / 10 - 1) / (2 * maxW * 9 / 10));
        const int target = total / pages;
        // the cut nearest the even share that still fits, preferring a clause end
        size_t best = 0;
        double bestCost = 1e30;
        for (size_t k = a + 1; k < b && (InName(f, k) || FitsPage(f, w, a, k)); ++k)
        {
            if (InName(f, k))
                continue;
            const int wk = f.width(Join(w, a, k), f.ctx);
            double cost = std::abs(wk - target);
            if (EndsClause(w[k - 1], false))
                cost -= maxW * 0.4;
            if (WeakWord(w[k - 1]))
                cost += maxW * 0.5;
            if (cost < bestCost)
                bestCost = cost, best = k;
        }
        if (!best) // nothing fits: one word per page, but the name whole
            best = std::max(a + 1, a < f.nameWords ? f.nameWords + 1 : 0);
        out.push_back({a, best});
        a = best;
    }
}

std::vector<std::pair<size_t, size_t>> PageSpans(const Fit& f, const std::vector<std::wstring>& w)
{
    std::vector<std::pair<size_t, size_t>> spans;
    size_t a = 0;
    while (a < w.size())
    {
        size_t e = std::max(a + 1, f.nameWords + 1); // a name ending in "." ("Dr.") is no sentence end
        while (e < w.size() && !EndsClause(w[e - 1], true))
            ++e;
        if (!FitsPage(f, w, a, e))
        {
            SplitEven(f, w, a, e, spans);
            a = e;
            continue;
        }
        // extend with further whole sentences while they fit
        for (;;)
        {
            size_t g = e + 1;
            if (e >= w.size())
                break;
            while (g < w.size() && !EndsClause(w[g - 1], true))
                ++g;
            if (!FitsPage(f, w, a, g))
                break;
            e = g;
        }
        spans.push_back({a, e});
        a = e;
    }
    return spans;
}
} // namespace linebreak
