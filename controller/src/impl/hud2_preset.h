// HUD 2's layout preset, the plumbing (docs\UI_HUD2_PRESETS.md 6 step 4): which preset the player asked for
// (`hud_preset`, controller.cpp kOptions), which layout draws (Standard's and Focus's exist - every other preset draws
// as Standard and says so), the preset's data (core\ui_meta.h uimeta::hud2p from UI_SPEC.json "hud2_presets":
// zones, boxed budgets, prompt caps, the value ids' homes) and the controller setup the preset adapts to (PRESETS 4:
// "HUD presets adapt to the controller preset" - here only the read; no behaviour reads it before step 5). Pure: no game
// call, no globals, no system headers (core\pre.h / common.h), so tests\hud2_preset_test.cpp runs the same code.
#pragma once
#include "common.h"
#include "ui_meta.h"

namespace hud2preset
{
namespace hp = uimeta::hud2p;
enum : int
{
    Standard = 0,
    Focus = 1,
    Speedrun = 2,
    Guided = 3,
};
constexpr int kCount = hp::kPresetCount;
static_assert(kCount == 4 && hp::kPresets[Standard].index == Standard && hp::kPresets[Guided].index == Guided,
              "uimeta::hud2p::kPresets in hud_preset order (tools\\ui_meta.py HUD2_PRESET_IDS)");
// The presets whose own layout is built. Today's HUD 2 is Standard's base (PRESETS 2.1 "Today's HUD 2, refined"); Focus
// (2.2) since 31 Oct 2026 (hud2_layout.h FocusCorners / Reveal, hud2_parts.cpp's Focus parts; UI_HUD2.md 5.18); Speedrun
// and Guided come with step 8, in order, the owner's frames between each. A preset turns true here only with its layout.
constexpr bool kBuilt[kCount] = {true, true, false, false};

// hud_preset -> a valid index: out of range = Standard (fail closed; the registry clamps 0..3 already, this guards a
// caller that passes something else).
inline int Clamp(int p) { return p >= 0 && p < kCount ? p : Standard; }
inline const hp::Preset& Data(int p) { return hp::kPresets[Clamp(p)]; }
inline bool Built(int p) { return kBuilt[Clamp(p)]; }
// The layout that draws: the preset's own once built, else Standard's.
inline int Drawn(int p) { return Built(p) ? Clamp(p) : Standard; }
// For `hud2 state` and the lint's header: "Standard", "Focus (layout not built: Standard drawn)".
inline std::string Label(int p)
{
    std::string s = Data(p).name;
    if (!Built(p))
        s += std::string(" (layout not built: ") + Data(Drawn(p)).name + " drawn)";
    return s;
}
// A value id's home under the preset (PRESETS 1.7 "one value, one place"): its off-screen home while the target is off
// screen when the spec names one, else its home; null = the preset does not show the value.
inline const char* Home(const hp::ValueId& v, int p, bool targetOff)
{
    const int i = Clamp(p);
    return targetOff && v.homeOff[i] ? v.homeOff[i] : v.home[i];
}

// ---- the controller setup the HUD preset adapts to (PRESETS 4)
struct Setup
{
    int padPreset = 0;          // pad_preset: 0 FREELANCER, 1 ACE, 2 ASSISTED, 3 CUSTOM (presets.h Preset)
    int weaponGroups = 1;       // weapon_groups 0 / 1 / 2
    bool holdsAsToggles = false; // holds_as_toggles
    bool kbm = false;           // keyboard and mouse is the input device
};
// What the setup requires in every HUD preset (PRESETS 4 rule 4, its table row by row; "a needed reading or prompt never
// disappears behind a preset's quietness"). The pad rows apply only while the pad is the device; K&M has its own row.
struct Needs
{
    bool throttleWord = false;       // FREELANCER / CUSTOM: the throttle word on change (the lever is used binary)
    bool killWord = false;           // FREELANCER / CUSTOM: KILL while engine kill holds
    bool throttleReadout = false;    // ACE (and K&M, PRESETS 2.2 / 4 compat): tick + % while the lever rests 1..99 %
    bool apLine = false;             // ASSISTED: the autopilot's line (mode . goal . waiting reason) while it flies
    bool takeControlPinned = false;  // ASSISTED: TAKE CONTROL (B) never retires
    bool reticleAlways = false;      // ASSISTED: the reticle always shown (RS aims)
    bool groupRows = false;          // weapon_groups 2: the two group rows with RT / LT badges in the fight form
    bool latchedWords = false;       // holds_as_toggles: a state word for every latched action
    bool keycaps = false;            // K&M: keycaps from the keymap, the click-pinned contact list, the maneuver row
                                     // or its tag, MINIMIZE HUD as the peek
};
// The setup's column of the compatibility table (CUSTOM = FREELANCER until the Bindings editor, PRESETS 4).
inline hp::Column ColumnOf(const Setup& s)
{
    if (s.kbm)
        return hp::kColKbm;
    switch (s.padPreset)
    {
    case 1: return hp::kColAce;
    case 2: return hp::kColAssisted;
    default: return hp::kColFreelancer;
    }
}
inline Needs NeedsOf(const Setup& s)
{
    Needs n;
    if (s.kbm)
    {
        n.keycaps = true;
        n.throttleReadout = true; // the K&M throttle rests anywhere (PRESETS 2.2 "K&M, ACE: 4"; 4 compat Focus / K&M)
        return n;
    }
    switch (ColumnOf(s))
    {
    case hp::kColAce: n.throttleReadout = true; break;
    case hp::kColAssisted: n.apLine = n.takeControlPinned = n.reticleAlways = true; break;
    default: n.throttleWord = n.killWord = true; break;
    }
    n.groupRows = s.weaponGroups == 2;
    n.latchedWords = s.holdsAsToggles;
    return n;
}
inline const char* FitName(hp::Fit f)
{
    switch (f)
    {
    case hp::Fit::Fine: return "fine";
    case hp::Fit::Adapts: return "adapts";
    case hp::Fit::Discouraged: return "discouraged";
    }
    return "?";
}
// The HUD preset suggested for the setup (PRESETS 4 "Suggested pairing, never forced").
inline int SuggestedFor(const Setup& s)
{
    return Clamp(s.kbm ? hp::kSuggestedKbm : hp::kSuggestedPad[s.padPreset >= 0 && s.padPreset < 4 ? s.padPreset : 0]);
}

// The read the layout core (step 5) adapts from: the setup, its column, the cell of the compatibility table for the
// asked-for preset, what the setup needs and the suggested pairing.
struct Adapt
{
    Setup setup;
    hp::Column column = hp::kColFreelancer;
    hp::Compat compat = {hp::Fit::Fine, false, ""};
    Needs needs;
    int suggested = Standard;
};
inline Adapt AdaptTo(int preset, const Setup& s)
{
    Adapt a;
    a.setup = s;
    a.column = ColumnOf(s);
    a.compat = hp::kCompat[Clamp(preset)][a.column];
    a.needs = NeedsOf(s);
    a.suggested = SuggestedFor(s);
    return a;
}
// One line for `hud2 state`: "setup=ACE groups=1 toggles=0 fit=adapts suggested=Standard needs=throttle_readout".
inline std::string Describe(const Adapt& a)
{
    std::string s = std::string("setup=") + hp::kColumnNames[a.column] + " groups=" + std::to_string(a.setup.weaponGroups) +
                    " toggles=" + (a.setup.holdsAsToggles ? "1" : "0") + " fit=" + FitName(a.compat.fit) +
                    (a.compat.suggested ? "(suggested)" : "") + " suggested=" + Data(a.suggested).name + " needs=";
    const struct
    {
        bool on;
        const char* name;
    } f[] = {{a.needs.throttleWord, "throttle_word"},   {a.needs.killWord, "kill_word"},
             {a.needs.throttleReadout, "throttle_readout"}, {a.needs.apLine, "ap_line"},
             {a.needs.takeControlPinned, "take_control"}, {a.needs.reticleAlways, "reticle"},
             {a.needs.groupRows, "group_rows"},       {a.needs.latchedWords, "latched_words"},
             {a.needs.keycaps, "kbm_keycaps"}};
    bool any = false;
    for (const auto& e : f)
        if (e.on)
            s += std::string(any ? "," : "") + e.name, any = true;
    if (!any)
        s += "-";
    return s;
}
} // namespace hud2preset
