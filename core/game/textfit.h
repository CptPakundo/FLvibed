// FLvibed - the ONE text-fit rule for every text FLvibed draws (fix-text, 25 Oct 2026; the vanilla 1.1 run cut texts
// with "..." everywhere): measure the string in its font at its size; when it does not fit its box, step the size down
// - continuously, only as far as needed - to the floor (the legibility floor below, MinSize: 0.024 H = HUD 2's text
// floor, never under 14 px; text_size sets the preferred size, never a smaller floor); only when it does not fit even
// there, cut it with an ellipsis at the floor (or wrap it, where a box takes more lines: hud2kit::FitLines). Text widths in FLvibed's renderers scale
// linearly with the size (core\game\render's scaled text, HUD 2's kit), so the fitting size is solved, not searched:
// w(size) = size * (sum of advances / em + track * (characters - 1)).
// Pure (no game, no device): the renderers pass each character's advance; results are cached per string, size, floor,
// box and track (Cache), so a box drawn every frame is measured once. Host-free tests: tests\textfit_test.cpp.
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace textfit
{
constexpr wchar_t kEllipsis = 0x2026;

// The legibility floor (the owner, 25 Oct 2026: legible at every font size and resolution) and the fit rule's floor: no
// text smaller than 0.024 H
// (HUD 2's text floor) nor than kMinPx pixels, whatever text_size / hud_scale say. 14 px = 0.024 H at 600 lines, the
// smallest screen the game's own resolution list offers (800x600); there Agency FB's bold caps stand ~10 px with the
// one-pixel outline - the smallest size it stays readable at (judged on the 720p / 4:3 frames of fix_text_2026-10-25).
constexpr float kMinPx = 14.0f;
inline float MinSize(float screenH)
{
    const float h = 0.024f * screenH;
    return h > kMinPx ? h : kMinPx;
}

struct Result
{
    std::wstring text; // the string, or its cut form ending in the ellipsis ("" when not even that fits)
    float size = 0;    // the size to draw it at (<= the asked size; >= the floor unless the asked size was below it)
    float width = 0;   // its width at that size
    bool shrunk = false, cut = false;
};

// The width of `chars` characters whose advances (at em `px`) sum to `raw`, drawn at `size` with `track` ems of extra
// spacing between characters.
inline float WidthAt(float raw, size_t chars, float px, float size, float track)
{
    return px > 0 ? raw * size / px + (chars > 1 ? track * size * static_cast<float>(chars - 1) : 0.0f) : 0.0f;
}

// The rule. `adv` = each character's advance at em `px` (adv.size() == s.size()), `ell` = the ellipsis' advance.
inline Result Fit(const std::wstring& s, const std::vector<float>& adv, float ell, float px, float size, float floor,
                  float maxW, float track)
{
    Result r;
    r.text = s, r.size = size;
    if (s.empty() || px <= 0 || size <= 0 || adv.size() != s.size())
        return r;
    float raw = 0;
    for (float a : adv)
        raw += a;
    const size_t n = s.size();
    r.width = WidthAt(raw, n, px, size, track);
    if (r.width <= maxW)
        return r;
    const float lo = floor < size ? floor : size; // never larger than asked
    const float per = WidthAt(raw, n, px, 1.0f, track); // the width per unit of size
    if (per > 0 && maxW > 0)
    {
        const float z = maxW / per * 0.999f; // (a hair under: no rounding past the box)
        if (z >= lo)
        {
            r.size = z, r.width = per * z, r.shrunk = true;
            return r;
        }
    }
    // the floor and still too wide: the longest prefix that fits with the ellipsis (trailing spaces dropped)
    r.size = lo, r.shrunk = lo < size, r.cut = true;
    size_t best = 0;
    float sum = 0;
    for (size_t k = 1; k <= n; ++k)
    {
        sum += adv[k - 1];
        if (WidthAt(sum + ell, k + 1, px, lo, track) > maxW)
            break;
        best = k;
    }
    while (best > 0 && (s[best - 1] == L' ' || s[best - 1] == 0x00B7))
        --best;
    if (best == 0)
    {
        const float we = WidthAt(ell, 1, px, lo, track);
        r.text = we <= maxW ? std::wstring(1, kEllipsis) : std::wstring();
        r.width = r.text.empty() ? 0 : we;
        return r;
    }
    r.text = s.substr(0, best) + kEllipsis;
    float w = ell;
    for (size_t k = 0; k < best; ++k)
        w += adv[k];
    r.width = WidthAt(w, best + 1, px, lo, track);
    return r;
}

// A word that grows in place (the subtitles' spoken-word pop), centred on its laid-out slot: the largest scale that
// keeps its word gaps. `restW` = its width as laid out, `baseW` = its width drawn at scale 1 in the grow font (~restW),
// `spaceW` = the space's advance; each side may take at most `share` of a space, so two neighbours growing at once
// still leave 60 % of the gap (fix-text 3, 25 Oct 2026: a fixed 1.08 grew a long word by more than Agency FB's narrow
// space - "yourcontrols", "showingthelocation"; a third of a space per side still read "troublewith" at 4K). Never above
// maxScale; under 1 only when the grow font's own width already overshoots the gap budget (hinting at another size).
inline float GrowCap(float restW, float baseW, float spaceW, float maxScale, float share = 0.2f)
{
    if (baseW <= 0 || restW <= 0)
        return 1.0f;
    const float cap = (restW + 2.0f * share * (spaceW > 0 ? spaceW : 0.0f)) / baseW;
    return cap > maxScale ? maxScale : cap;
}

// Results per (string, size, floor, box, track, font id), the floats quantised to 1/8 px. References stay valid until
// Clear / Bound: the owner bounds it at a pass's start (no reference lives across passes) and clears it when its fonts
// are rebuilt.
class Cache
{
  public:
    const Result* Find(const std::wstring& s, float size, float floor, float maxW, float track, unsigned font)
    {
        Key(s, size, floor, maxW, track, font);
        auto it = map_.find(key_);
        return it == map_.end() ? nullptr : &it->second;
    }
    const Result& Put(Result r) { return map_[key_] = std::move(r); } // (under the key of the last Find)
    void Bound(size_t n = 2048)
    {
        if (map_.size() > n)
            map_.clear();
    }
    void Clear() { map_.clear(); }
    size_t Size() const { return map_.size(); }

  private:
    void Key(const std::wstring& s, float size, float floor, float maxW, float track, unsigned font)
    {
        key_.assign(s);
        key_ += L'\x1F';
        auto q = [&](float v)
        {
            const long long i = static_cast<long long>(v * 8.0f + (v >= 0 ? 0.5f : -0.5f));
            key_ += static_cast<wchar_t>(i & 0xFFFF);
            key_ += static_cast<wchar_t>((i >> 16) & 0xFFFF);
        };
        q(size), q(floor), q(maxW), q(track * 1000.0f);
        key_ += static_cast<wchar_t>(font & 0xFFFF);
        key_ += static_cast<wchar_t>(font >> 16);
    }
    std::unordered_map<std::wstring, Result> map_;
    std::wstring key_;
};
} // namespace textfit
