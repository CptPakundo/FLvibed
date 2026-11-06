// FLvibed - the options page's rules, pure: no game call, so tests\options_rules_test.cpp runs the code the page runs (docs\UI_OPTIONS.md 6).
// What a head row reads from its keys, what acting on it writes, what lists under YOU CHANGED, which preset word a bundle's rows make, how the rows' words
// name a button. core\ui\options_page.cpp holds the drawing and the reads; every decision about those stays here.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace optrules
{
// Held direction: the first repeat, then eight a second.
constexpr float kRepeatFirst = 0.4f, kRepeatNext = 0.125f;

// Two registry values the same? Numbers compare as numbers ("1" == "1.0", "0.35" == "0.350000").
inline bool SameValue(const std::string& a, const std::string& b)
{
    if (a == b)
        return true;
    char* ea = nullptr;
    char* eb = nullptr;
    const double da = strtod(a.c_str(), &ea), db = strtod(b.c_str(), &eb);
    if (ea == a.c_str() || eb == b.c_str() || *ea || *eb)
        return false; // not both numbers
    return std::fabs(da - db) < 1e-6 * std::max(1.0, std::fabs(da));
}

// ------------------------------------------------------------------ the head row
enum class Head : int
{
    Enabled,
    Disabled,
    DisabledAtNextStart, // off, the feature runs until the next start
    EnabledAtNextStart,  // on, the feature was off at this start
    Stopped              // a fault stopped it: nothing to set until the game restarts
};

struct HeadKeys
{
    bool twoKeys = false; // a live switch beside the module row: Subtitles, HUD 2
    bool live = false;    // that switch is on
    bool want = false;    // the [Modules] row: what the next start runs
    bool running = false; // what this start runs (hostcalls ModuleOn)
    bool fault = false;   // its slot stopped after a fault
};

// A two-key head reads ENABLED only with both keys on, and its off acts at once: it never waits for a start going off, and a saved mix reads DISABLED.
inline Head HeadState(const HeadKeys& k)
{
    if (k.running && k.fault)
        return Head::Stopped;
    const bool on = k.want && (!k.twoKeys || k.live);
    if (k.twoKeys)
        return !on ? Head::Disabled : k.running ? Head::Enabled : Head::EnabledAtNextStart;
    if (k.running)
        return on ? Head::Enabled : Head::DisabledAtNextStart;
    return on ? Head::EnabledAtNextStart : Head::Disabled;
}

// The tab's amber mark: the row differs from what is running, so a restart is what the player waits for.
inline bool Waits(Head h) { return h == Head::DisabledAtNextStart || h == Head::EnabledAtNextStart; }
// The rows under a head are live only with it ENABLED; every other state dims them.
inline bool RowsLive(Head h) { return h == Head::Enabled; }
// A stopped head has no box and takes no input.
inline bool Takes(Head h) { return h != Head::Stopped; }

// What acting on a head writes.
struct HeadPlan
{
    bool ask = false;             // the Controller going off asks first: nothing else is set
    bool setLive = false, live = false;
    bool setModule = false, module = false;
    bool subtitlesStayLoaded = false; // the other side of the rule below: the Subtitles row on, `enabled` off
};

enum class Feature : int
{
    Subtitles,
    Controller,
    Other
};

// FLvibed starts only with the Subtitles or the Controller module on. A player who disables both keeps the Subtitles module loaded with `enabled` 0: nothing shows, every
// gate in the subtitles' display reads `enabled`, and the head reads DISABLED. `otherWant`: the other of the two rows. `confirmed`: the Controller's dialog was answered.
inline HeadPlan PlanHead(Feature feature, bool twoKeys, bool turnOn, bool otherWant, bool confirmed)
{
    HeadPlan p;
    if (feature == Feature::Controller && !turnOn && !confirmed)
    {
        p.ask = true;
        return p;
    }
    p.setLive = twoKeys;
    p.live = turnOn;
    p.setModule = true;
    p.module = turnOn;
    if (!turnOn && !otherWant)
    {
        if (feature == Feature::Subtitles)
            p.setModule = false;
        else if (feature == Feature::Controller)
            p.subtitlesStayLoaded = true;
    }
    return p;
}

// ------------------------------------------------------------------ YOU CHANGED
// A hidden row away from its default comes back on its tab. A member of a preset or bundle that reads a named word stays out: the player chose the word, and its row says so.
inline bool ListChanged(bool hidden, bool differs, bool namedByPreset) { return hidden && differs && !namedByPreset; }

// ------------------------------------------------------------------ presets and bundles
// The option a preset row reads: the first preset whose every member holds its value, else `custom`. `chosen` >= 0: a real row names its preset, only that one can match.
// A member no module owns right now cannot be set, so it does not decide; a preset with no member left does not match. get(member) -> the value, has(member) -> owned.
template <class Preset, class Get, class Has>
int PresetWord(const Preset* presets, int count, int custom, int chosen, Get get, Has has)
{
    for (int i = 0; i < count && i < custom; ++i)
    {
        if (chosen >= 0 && chosen != i)
            continue;
        bool any = false, all = true;
        for (int m = 0; m < presets[i].count && all; ++m)
        {
            if (!has(presets[i].values[m]))
                continue;
            any = true;
            all = SameValue(get(presets[i].values[m]), presets[i].values[m].value);
        }
        if (any && all)
            return i;
    }
    return custom;
}

// ------------------------------------------------------------------ button names in words
enum Family : int // glyphs::Family's order
{
    Xbox,
    PlayStation,
    Nintendo,
    Steam
};

// A face button in the family's words, positional as glyphs::Id (A south, B east, X west, Y north).
inline const char* FaceName(char button, int family)
{
    static const char* const kPlayStation[] = {"Cross", "Circle", "Square", "Triangle"};
    static const char* const kNintendo[] = {"B", "A", "Y", "X"};
    static const char* const kPlain[] = {"A", "B", "X", "Y"};
    const int i = button == 'A' ? 0 : button == 'B' ? 1 : button == 'X' ? 2 : 3;
    return family == PlayStation ? kPlayStation[i] : family == Nintendo ? kNintendo[i] : kPlain[i];
}

// The spec's "{A}" "{B}" "{X}" "{Y}" in the family's words; `pad` false: {A} is the Enter key, what opens a row on the keyboard. `upper`: the words of a value.
inline std::string ExpandButtons(const std::string& text, int family, bool pad, bool upper)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char b = i + 2 < text.size() && text[i] == '{' && text[i + 2] == '}' ? text[i + 1] : 0;
        if (b != 'A' && b != 'B' && b != 'X' && b != 'Y')
        {
            out += text[i];
            continue;
        }
        std::string word = b == 'A' && !pad ? "Enter" : FaceName(b, family);
        if (upper)
            for (char& c : word)
                c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        out += word;
        i += 2;
    }
    return out;
}
} // namespace optrules
