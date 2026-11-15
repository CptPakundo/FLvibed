// FLvibed - the STATUS page's rules, pure (the HUD 2 page framework, audit 70b F4b; moved from modules\hud\src\nnpage_rules.h,
// whose names stay as using-declarations: INFO's factions use the standings too): the game's five standings, the reputation
// list's filter, how far the player's worth is on the way to the next level, a heading without its colon, and the words the
// pages say them in. No game include: tests\nnpage_test.cpp holds them.
#pragma once
#include <algorithm>
#include <string>

namespace statusrules
{
// The game's own five steps (0x45A6E0): at or under -0.6, at or under -0.2, up to 0.2, under 0.6, from 0.6.
enum Band
{
    Hostile,
    Unfriendly,
    Neutral,
    Warm,
    Friendly
};
inline Band BandOf(float v) { return v <= -0.6f ? Hostile : v <= -0.2f ? Unfriendly : v >= 0.6f ? Friendly : v > 0.2f ? Warm : Neutral; }

// The reputation list's filter, one press each: all, then who shoots, who does not care, who likes the player.
enum Filter
{
    All,
    Enemies,
    Indifferent,
    Friends,
    kFilters
};
inline bool Passes(Filter f, float v)
{
    const Band b = BandOf(v);
    return f == All || (f == Enemies && b <= Unfriendly) || (f == Indifferent && b == Neutral) || (f == Friends && b >= Warm);
}

// How far the player's worth is on the way to the next level: 0 .. 1; 0 when the game names no next level.
inline float Share(long long worth, long long next) { return next <= 0 ? 0 : std::max(0.0f, std::min(1.0f, static_cast<float>(worth) / static_cast<float>(next))); }

// A heading of the game's without its colon.
inline std::wstring Heading(std::wstring s)
{
    while (!s.empty() && (s.back() == L':' || s.back() == L' '))
        s.pop_back();
    return s;
}

// The words: a standing's (its colour: pages\page_kit.h BandRgb), a filter's.
inline const wchar_t* BandWord(Band b)
{
    static const wchar_t* const kWords[] = {L"HOSTILE", L"UNFRIENDLY", L"NEUTRAL", L"FAVOURABLE", L"FRIENDLY"};
    return kWords[b];
}
inline const wchar_t* FilterWord(Filter f)
{
    static const wchar_t* const kWords[] = {L"ALL", L"ENEMIES", L"INDIFFERENT", L"FRIENDS"};
    return kWords[f];
}
// The filter one press on (the pad's X, the mouse's SHOW chip).
inline Filter NextFilter(Filter f) { return static_cast<Filter>((f + 1) % kFilters); }
} // namespace statusrules
