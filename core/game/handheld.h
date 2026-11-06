// FLvibed - is this machine a handheld (modules\hud\docs\UI_HUD2_INPUTS.md 7)? `hud_size` Automatic asks once. The chain, ranked: the
// SMBIOS model table (a Deck under Proton reads its name from any launcher, because Wine builds the table from the machine's own), the
// Windows device form with a small integrated panel, a small panel alone, Steam's variable under Wine, then exactly 1280x800.
// Rejected on purpose: the chassis type, tablet flags, a maker alone (`handhelds.inc` says which names need the maker).
// Pure: handheld.cpp reads the machine into Facts; ParseSmbios and Decide run the same in tests\handheld_test.cpp. No game call.
#pragma once
#include "game/hudsize.h"

#include <string>
#include <vector>

namespace handheld
{
// What the machine says. Every field is optional: an absent fact leaves its rung of the chain out.
struct Facts
{
    std::string systemMaker, systemProduct, boardMaker, boardProduct; // SMBIOS type 1 and type 2
    bool wine = false;           // ntdll exports wine_get_version
    bool steamDeckEnv = false;   // SteamDeck=1 in the environment
    bool familyHandheld = false; // RtlGetDeviceFamilyInfoEnum's form 0x2E
    float panelInches = 0;       // the integrated panel's diagonal, 0 = none found or unreadable
    bool externalMonitor = false; // Windows: the game's monitor is not the integrated panel
    float W = 0, H = 0;          // the game's screen
};

struct Verdict
{
    int preset = hudsize::Normal; // Normal or Large
    const char* why = "default";  // for the log
};

enum Field : int
{
    System,
    Board,
    Either
};
enum Match : int
{
    Exact,
    Prefix
};
enum Kind : int
{
    NotHandheld,
    Handheld,
    Deck
};
struct Row
{
    Field field;
    Match match;
    const char* name;
    const char* maker; // "" any
    Kind kind;
};
inline const Row kRows[] = {
#include "game/handhelds.inc"
};

inline char Lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

// `s` starts with `p`, either case.
inline bool StartsWith(const std::string& s, const char* p)
{
    size_t i = 0;
    for (; p[i]; ++i)
        if (i >= s.size() || Lower(s[i]) != Lower(p[i]))
            return false;
    return true;
}

inline bool Same(const std::string& s, const char* p)
{
    size_t n = 0;
    while (p[n])
        ++n;
    return s.size() == n && StartsWith(s, p);
}

inline bool Named(const std::string& product, const Row& r)
{
    return !product.empty() && (r.match == Exact ? Same(product, r.name) : StartsWith(product, r.name));
}

// The first row of the table the facts match; nullptr none.
inline const Row* Find(const Facts& f)
{
    for (const Row& r : kRows)
    {
        const bool system = r.field != Board && Named(f.systemProduct, r) && (!*r.maker || StartsWith(f.systemMaker, r.maker));
        const bool board = r.field != System && Named(f.boardProduct, r) && (!*r.maker || StartsWith(f.boardMaker, r.maker));
        if (system || board)
            return &r;
    }
    return nullptr;
}

constexpr float kFamilyPanelInches = 13.0f, kSmallPanelInches = 9.0f;

// The chain for Automatic. A failed read leaves a rung out, and the last rung is Normal, so nothing here can fail open.
inline Verdict Decide(const Facts& f)
{
    const bool exactDeckScreen = f.W == 1280.0f && f.H == 800.0f;
    if (const Row* r = Find(f))
    {
        if (r->kind == NotHandheld)
            return {hudsize::Normal, "not a handheld, by model"};
        // docked: no API tells. A monitor that is not the integrated panel says so on Windows; a Deck on a screen over 800 lines
        // is taken for docked, as Valve's own scaling on televisions does
        if (f.externalMonitor)
            return {hudsize::Normal, "a handheld on an external monitor"};
        if (r->kind == Deck && f.H > hudsize::kHandheldH)
            return {hudsize::Normal, "a Steam Deck over 800 lines: docked"};
        return {hudsize::Large, r->kind == Deck ? "a Steam Deck, by model" : "a handheld, by model"};
    }
    if (!f.externalMonitor && f.familyHandheld && f.panelInches > 0 && f.panelInches <= kFamilyPanelInches)
        return {hudsize::Large, "the device form with a small panel"};
    if (!f.externalMonitor && f.panelInches > 0 && f.panelInches <= kSmallPanelInches)
        return {hudsize::Large, "an integrated panel of 9 inches or less"};
    if (f.wine && f.steamDeckEnv && f.systemProduct.empty() && f.boardProduct.empty())
        return {hudsize::Large, "Steam's variable under Wine"};
    if (exactDeckScreen)
        return {hudsize::Large, "exactly 1280x800"};
    return {hudsize::Normal, "no evidence of a handheld"};
}

// One string of an SMBIOS structure: `index` is 1-based into the strings after the formatted area, 0 = none.
inline std::string SmbiosString(const unsigned char* strings, const unsigned char* end, int index)
{
    if (index <= 0)
        return std::string();
    const unsigned char* p = strings;
    for (int i = 1; p < end && *p; ++i)
    {
        const unsigned char* q = p;
        while (q < end && *q)
            ++q;
        if (i == index)
            return std::string(reinterpret_cast<const char*>(p), static_cast<size_t>(q - p));
        p = q + 1;
    }
    return std::string();
}

// Reads the makers and products out of the buffer GetSystemFirmwareTable('RSMB') fills: an 8-byte header, then structures, each a
// type, a length, a handle, the formatted area and its strings ending in a double zero. Stops at a malformed structure.
inline void ParseSmbios(const unsigned char* data, size_t size, Facts& f)
{
    if (!data || size < 8)
        return;
    const unsigned char* p = data + 8;
    const unsigned char* end = data + size;
    while (p + 4 <= end)
    {
        const int type = p[0], len = p[1];
        if (len < 4 || p + len > end)
            return;
        const unsigned char* strings = p + len;
        if (len > 5 && (type == 1 || type == 2))
        {
            std::string& maker = type == 1 ? f.systemMaker : f.boardMaker;
            std::string& product = type == 1 ? f.systemProduct : f.boardProduct;
            maker = SmbiosString(strings, end, p[4]);
            product = SmbiosString(strings, end, p[5]);
        }
        if (type == 127)
            return;
        const unsigned char* q = strings;
        while (q + 1 < end && !(q[0] == 0 && q[1] == 0))
            ++q;
        p = q + 2;
    }
}

// The machine's facts (handheld.cpp), once: the screen's fields stay the caller's. Never call it from DllMain.
Facts Read();
} // namespace handheld
