// FLvibed - a ship's hardpoints and equipment numbers as rules: which hardpoint takes what, which are free, how two items compare. Pure, header only; tested by
// tests\equip_rules_test.cpp. The data comes from core\game\gamedata.* (shiparch.ini hp_type lines, the equipment inis); the pages draw it (modules\hud\src\nnpage.*).
#pragma once
#include "common.h"

#include <algorithm>
#include <string>
#include <vector>

namespace equiprules
{
// One hardpoint and every type a ship's hp_type lines let it take.
struct Hp
{
    std::string name;
    std::vector<std::string> types; // lower case
};

inline std::string Lower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

// A family and a class from a type: hp_gun_special_3 -> "gun", 3; hp_thruster -> "thruster", 0. Empty family: not a slot the player sees.
struct Slot
{
    const wchar_t* word = nullptr; // upper case, the family as the player reads it
    int cls = 0;
    int filter = 0; // the inventory's filter that lists its items: 0 weapons (the game lists mines and countermeasures there too), 2 external equipment
    int rank = 99;  // the order rows take: guns first
};
inline Slot SlotOf(const std::string& type)
{
    const struct
    {
        const char* prefix;
        const wchar_t* word;
        int filter, rank;
    } kFamilies[] = {
        {"hp_gun_special_", L"GUN/MISSILE", 0, 0},
        {"hp_turret_special_", L"TURRET", 0, 1},
        {"hp_torpedo_special_", L"TORPEDO", 0, 2},
        {"hp_fighter_shield_special_", L"LIGHT SHIELD", 2, 3},
        {"hp_elite_shield_special_", L"HEAVY SHIELD", 2, 3},
        {"hp_freighter_shield_special_", L"FREIGHTER SHIELD", 2, 3},
        {"hp_thruster", L"THRUSTER", 2, 4},
        {"hp_countermeasure_dropper", L"COUNTERMEASURE", 0, 5},
        {"hp_mine_dropper", L"MINE", 0, 6},
    };
    Slot s;
    for (const auto& f : kFamilies)
    {
        const size_t n = strlen(f.prefix);
        if (type.compare(0, n, f.prefix) == 0)
        {
            s.word = f.word, s.filter = f.filter, s.rank = f.rank;
            if (type.size() > n)
                s.cls = atoi(type.c_str() + n);
            return s;
        }
    }
    return s;
}

// A hardpoint's best type: the family it lists first by rank, the highest class of that family.
inline Slot BestOf(const Hp& hp)
{
    Slot best;
    for (const std::string& t : hp.types)
    {
        const Slot s = SlotOf(t);
        if (!s.word)
            continue;
        if (!best.word || s.rank < best.rank || (s.rank == best.rank && s.cls > best.cls))
            best = s;
    }
    return best;
}
// "GUN/MISSILE CLASS 3", "THRUSTER": the game's own row words for an empty slot.
inline std::wstring SlotWord(const Slot& s)
{
    if (!s.word)
        return L"";
    std::wstring w = s.word;
    if (s.cls > 0)
        w += L" CLASS " + std::to_wstring(s.cls);
    return w;
}

// One line of shiparch.ini: "hp_gun_special_3, HpWeapon01, HpWeapon02".
inline void AddLine(std::vector<Hp>& hps, const std::vector<std::string>& fields)
{
    if (fields.size() < 2)
        return;
    const std::string type = Lower(fields[0]);
    for (size_t i = 1; i < fields.size(); ++i)
    {
        const std::string name = Lower(fields[i]);
        auto it = std::find_if(hps.begin(), hps.end(), [&](const Hp& h) { return h.name == name; });
        if (it == hps.end())
        {
            hps.push_back({name, {}});
            it = hps.end() - 1;
        }
        if (std::find(it->types.begin(), it->types.end(), type) == it->types.end())
            it->types.push_back(type);
    }
}

inline bool Accepts(const Hp& hp, const std::string& type) { return std::find(hp.types.begin(), hp.types.end(), Lower(type)) != hp.types.end(); }

// The hardpoints nothing is mounted on, in the rows' order; `filter` -1 all.
inline std::vector<Hp> Free(const std::vector<Hp>& hps, const std::vector<std::string>& used, int filter)
{
    std::vector<Hp> out;
    for (const Hp& h : hps)
    {
        const Slot s = BestOf(h);
        if (!s.word || (filter >= 0 && s.filter != filter))
            continue;
        bool taken = false;
        for (const std::string& u : used)
            taken = taken || Lower(u) == h.name;
        if (!taken)
            out.push_back(h);
    }
    std::stable_sort(out.begin(), out.end(), [](const Hp& a, const Hp& b)
    {
        const Slot x = BestOf(a), y = BestOf(b);
        return x.rank != y.rank ? x.rank < y.rank : x.cls > y.cls;
    });
    return out;
}

// A hardpoint name before another by its own number: HpWeapon02 before HpWeapon10 (the trailing digits as a number, the rest as text).
inline bool SlotNameLess(const std::string& a, const std::string& b)
{
    auto split = [](const std::string& s, std::string& head, long& n)
    {
        size_t i = s.size();
        while (i > 0 && isdigit(static_cast<unsigned char>(s[i - 1])))
            --i;
        head = s.substr(0, i);
        n = i < s.size() ? atol(s.c_str() + i) : -1;
    };
    std::string ha, hb;
    long na = 0, nb = 0;
    split(a, ha, na), split(b, hb, nb);
    return ha != hb ? ha < hb : na < nb;
}

// The ship's hardpoints a filter shows, in the ship's own order: its kinds as its shiparch lines first name them, each kind's slots by their own numbers (HpWeapon01, 02,
// 03 - the lines go by gun class, the Starflier's first names HpWeapon03 alone). Never an order of ours: a slot keeps its place whatever is on it, its place is part of the
// ship (the owner, issue 52).
inline std::vector<Hp> Slots(const std::vector<Hp>& hps, int filter)
{
    std::vector<Hp> out;
    std::vector<int> kinds; // the kinds' ranks, in the order the lines first name them
    for (const Hp& h : hps)
    {
        const Slot s = BestOf(h);
        if (!s.word || (filter >= 0 && s.filter != filter))
            continue;
        out.push_back(h);
        if (std::find(kinds.begin(), kinds.end(), s.rank) == kinds.end())
            kinds.push_back(s.rank);
    }
    auto kindOf = [&](const Hp& h) { return std::find(kinds.begin(), kinds.end(), BestOf(h).rank) - kinds.begin(); };
    std::stable_sort(out.begin(), out.end(), [&](const Hp& a, const Hp& b)
    {
        const auto ka = kindOf(a), kb = kindOf(b);
        return ka != kb ? ka < kb : SlotNameLess(a.name, b.name);
    });
    return out;
}

// One number of an item. `better`: +1 more is better, -1 less is better, 0 neither.
struct Stat
{
    const wchar_t* label = L"";
    float value = 0;
    int better = 0;
    const wchar_t* unit = L"";
};
struct Spec
{
    std::string type; // its hardpoint type, lower case; "" none (a commodity, an internal)
    std::vector<Stat> stats;
};

enum class Verdict
{
    Same,
    Better,
    Worse,
    Other // no way to say
};
inline Verdict Judge(const Stat& mine, float theirs)
{
    const float d = mine.value - theirs, tiny = 0.005f * std::max(1.0f, fabsf(theirs));
    if (fabsf(d) <= tiny)
        return Verdict::Same;
    if (mine.better == 0)
        return Verdict::Other;
    return (d > 0) == (mine.better > 0) ? Verdict::Better : Verdict::Worse;
}
// The value in the same label of `of`; false none.
inline bool Find(const std::vector<Stat>& of, const wchar_t* label, float& v)
{
    for (const Stat& s : of)
        if (wcscmp(s.label, label) == 0)
        {
            v = s.value;
            return true;
        }
    return false;
}
// "+18 %", "-120", "=" : a delta in the words of a row.
inline std::wstring DeltaWord(float mine, float theirs)
{
    const float d = mine - theirs;
    if (fabsf(d) <= 0.005f * std::max(1.0f, fabsf(theirs)))
        return L"=";
    wchar_t b[32];
    if (fabsf(theirs) > 0.5f)
        swprintf_s(b, L"%+.0f %%", 100 * d / fabsf(theirs));
    else
        swprintf_s(b, L"%+.0f", d);
    return b;
}
// A number in a row: no decimals from 100, one below, none on a whole number.
inline std::wstring ValueWord(float v)
{
    wchar_t b[32];
    if (fabsf(v) >= 100 || fabsf(v - roundf(v)) < 0.05f)
        swprintf_s(b, L"%.0f", v);
    else
        swprintf_s(b, L"%.1f", v);
    return b;
}
} // namespace equiprules
