// FLvibed - a pilot's name past the game's cut (docs\FREELANCER_RE.md "NPC names"): the game formats a name into 24 characters and caches that, so what it
// shows stops at 23. A name record may hold the name whole, without any prefix the cached text carries ("Escort 1 ", "Transport DSE "); an encounter's pilot has only a call sign there, and the game's surnames finish the word. Pure: tests\namecut_test.cpp.
#pragma once
#include <algorithm>
#include <string>
#include <vector>

namespace namecut
{
constexpr size_t kCut = 23;   // the characters the game's name buffer holds
constexpr size_t kOverlap = 4; // the least shared text that says the two are one name

// The record's template runs on past the name, "%s0 %s1 %s2 %d0", with a call sign the game's 24 characters usually cut: " Black 17", or " 0" with no word. The name alone.
inline std::wstring Bare(const std::wstring& full)
{
    size_t digits = full.size();
    bool zero = true;
    while (digits > 0 && full[digits - 1] >= L'0' && full[digits - 1] <= L'9')
    {
        --digits;
        zero = zero && full[digits] == L'0';
    }
    if (digits == full.size() || digits == 0 || full[digits - 1] != L' ')
        return full;
    size_t end = digits;
    while (end > 0 && full[end - 1] == L' ')
        --end;
    if (!zero)
    {
        while (end > 0 && full[end - 1] != L' ') // (the call sign's word)
            --end;
        while (end > 0 && full[end - 1] == L' ')
            --end;
    }
    return end ? full.substr(0, end) : full;
}

// `cached` as the game shows it, `record` from the name record. The cached text, its cut tail replaced by the record, when that tail starts the record; else the
// cached text whole: a name set by a mission has no record to agree with.
inline std::wstring Join(const std::wstring& cached, const std::wstring& record)
{
    const std::wstring full = Bare(record);
    if (cached.empty())
        return full;
    if (cached.size() < kCut || full.size() <= kOverlap)
        return cached;
    for (size_t at = 0; at + kOverlap <= cached.size(); ++at)
    {
        const size_t n = cached.size() - at, in = full.find(cached.substr(at)); // (the record may hold a first name the cache dropped: anywhere in it)
        if (in != std::wstring::npos && in + n < full.size())
            return cached.substr(0, at) + full.substr(in);
    }
    return cached;
}
// A pilot of an encounter has no name in the record, which reads "Rogues Raiders Silver 12": rank, first and last name stand only in the cached text, cut at 23. Its last word
// completes from the game's surnames (`sorted`, faction_prop.ini `lastname`), when it is no surname itself and just one starts with it; else the text stays as it is.
inline std::wstring Complete(const std::wstring& cached, const std::vector<std::wstring>& sorted)
{
    if (cached.size() < kCut)
        return cached;
    const size_t space = cached.rfind(L' ');
    const std::wstring word = space == std::wstring::npos ? cached : cached.substr(space + 1);
    if (word.size() < 2 || std::binary_search(sorted.begin(), sorted.end(), word))
        return cached;
    const auto first = std::lower_bound(sorted.begin(), sorted.end(), word);
    if (first == sorted.end() || first->compare(0, word.size(), word) != 0)
        return cached;
    const auto next = first + 1;
    if (next != sorted.end() && next->compare(0, word.size(), word) == 0)
        return cached; // (two surnames fit: the cut stands, a wrong name is worse)
    return cached + first->substr(word.size());
}
} // namespace namecut
