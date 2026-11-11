// The options page's pure rules (core\ui\options_rules.h, docs\UI_OPTIONS.md 6) and the spec's tables they run on: the head's two-key state table, what acting on a head
// writes, the comparison with what is running, the changed-row rule, the bundles and CUSTOM, the first card's stamp, the button words, the layout card's lines.
#include "ui/options_rules.h"

#include "modules_rules.h"
#include "test.h"
#include "ui/presets.h"
#include "ui_meta.h"

#include <windows.h>

#include <map>

#include "settings_reset.h"

namespace
{
using optrules::Feature;
using optrules::Head;
using optrules::HeadKeys;
using optrules::HeadState;
using optrules::PlanHead;

HeadKeys One(bool want, bool running)
{
    HeadKeys k;
    k.want = want, k.running = running;
    return k;
}

HeadKeys Two(bool live, bool want, bool running)
{
    HeadKeys k;
    k.twoKeys = true, k.live = live, k.want = want, k.running = running;
    return k;
}

void HeadTable()
{
    // one key: the row against what is running
    CHECK(HeadState(One(true, true)) == Head::Enabled);
    CHECK(HeadState(One(false, true)) == Head::DisabledAtNextStart);
    CHECK(HeadState(One(true, false)) == Head::EnabledAtNextStart);
    CHECK(HeadState(One(false, false)) == Head::Disabled);
    // two keys: off acts at once, ENABLED needs both, a mix reads DISABLED, only coming back waits
    CHECK(HeadState(Two(true, true, true)) == Head::Enabled);
    CHECK(HeadState(Two(false, false, true)) == Head::Disabled);
    CHECK(HeadState(Two(false, true, true)) == Head::Disabled); // the Classic player of today, the module on
    CHECK(HeadState(Two(true, false, true)) == Head::Disabled);
    CHECK(HeadState(Two(true, true, false)) == Head::EnabledAtNextStart);
    CHECK(HeadState(Two(false, true, false)) == Head::Disabled); // the module kept loaded by the rule below, `enabled` 0
    CHECK(HeadState(Two(true, false, false)) == Head::Disabled);
    // a fault outranks everything while the module runs; an off module has nothing to stop
    HeadKeys k = One(true, true);
    k.fault = true;
    CHECK(HeadState(k) == Head::Stopped);
    k.want = false;
    CHECK(HeadState(k) == Head::Stopped);
    k = Two(true, true, true);
    k.fault = true;
    CHECK(HeadState(k) == Head::Stopped);
    k = One(true, false);
    k.fault = true;
    CHECK(HeadState(k) == Head::EnabledAtNextStart);
}

void RestartComparison()
{
    CHECK(optrules::Waits(Head::DisabledAtNextStart));
    CHECK(optrules::Waits(Head::EnabledAtNextStart));
    CHECK(!optrules::Waits(Head::Enabled));
    CHECK(!optrules::Waits(Head::Disabled));
    CHECK(!optrules::Waits(Head::Stopped));
    // going back cancels it: the row equals what runs, so nothing waits
    CHECK(!optrules::Waits(HeadState(One(true, true))));
    CHECK(!optrules::Waits(HeadState(One(false, false))));
    // a two-key head's off never waits: it acted at once
    CHECK(!optrules::Waits(HeadState(Two(false, false, true))));
    // only ENABLED leaves the rows live; a stopped head takes nothing
    CHECK(optrules::RowsLive(Head::Enabled));
    for (const Head h : {Head::Disabled, Head::DisabledAtNextStart, Head::EnabledAtNextStart, Head::Stopped})
        CHECK(!optrules::RowsLive(h));
    CHECK(!optrules::Takes(Head::Stopped));
    CHECK(optrules::Takes(Head::DisabledAtNextStart));
}

void HeadWrites()
{
    // a two-key head writes both, on and off
    auto p = PlanHead(Feature::Other, true, false, true, false);
    CHECK(p.setLive && !p.live && p.setModule && !p.module && !p.ask);
    p = PlanHead(Feature::Subtitles, true, true, true, false);
    CHECK(p.setLive && p.live && p.setModule && p.module);
    // a one-key head writes the module row alone
    p = PlanHead(Feature::Other, false, false, true, false);
    CHECK(!p.setLive && p.setModule && !p.module);
    // only the Controller going off asks; once answered it writes
    CHECK(PlanHead(Feature::Controller, false, false, true, false).ask);
    CHECK(!PlanHead(Feature::Controller, false, true, true, false).ask);
    CHECK(!PlanHead(Feature::Subtitles, true, false, true, false).ask);
    p = PlanHead(Feature::Controller, false, false, true, true);
    CHECK(!p.ask && p.setModule && !p.module && !p.subtitlesStayLoaded);
    // one of Subtitles and Controller stays: the Subtitles head goes off with the Controller off, so its module stays loaded, `enabled` 0
    p = PlanHead(Feature::Subtitles, true, false, false, false);
    CHECK(p.setLive && !p.live && !p.setModule);
    // the Controller goes off with the Subtitles off: the Subtitles row comes back on, `enabled` 0
    p = PlanHead(Feature::Controller, false, false, false, true);
    CHECK(p.setModule && !p.module && p.subtitlesStayLoaded);
    // what the next start then runs: never nothing, and the head still reads DISABLED
    CHECK(HeadState(Two(false, true, true)) == Head::Disabled);
}

void ChangedRows()
{
    CHECK(optrules::ListChanged(true, true, false));
    CHECK(!optrules::ListChanged(false, true, false)); // on the page already
    CHECK(!optrules::ListChanged(true, false, false)); // back at its default it leaves
    CHECK(!optrules::ListChanged(true, true, true));   // a member of a preset that reads a word
    CHECK(optrules::SameValue("1", "1.0"));
    CHECK(optrules::SameValue("0.35", "0.350000"));
    CHECK(!optrules::SameValue("0.5", "1"));
    CHECK(!optrules::SameValue("K", "L"));
}

const uimeta::PresetSet& Set(const char* key)
{
    const uimeta::PresetSet* s = uimeta::FindPresets(key);
    static const uimeta::PresetSet none = {"", nullptr, 0};
    CHECK(s != nullptr);
    return s ? *s : none;
}

using Values = std::map<std::string, std::string>;

int Word(const uimeta::PresetSet& s, const Values& v, int custom, int chosen = -1)
{
    const auto get = [&v](const uimeta::PresetValue& m) { const auto i = v.find(m.key); return i == v.end() ? std::string() : i->second; };
    const auto has = [&v](const uimeta::PresetValue& m) { return v.count(m.key) != 0; };
    return optrules::PresetWord(s.presets, s.count, custom, chosen, get, has);
}

Values Apply(const uimeta::Preset& p)
{
    Values v;
    for (int i = 0; i < p.count; ++i)
        v[p.values[i].key] = p.values[i].value;
    return v;
}

// a bundle's table against docs\UI_OPTIONS.md 6.2, through the spec's words and the rows' codings
void CheckMember(const uimeta::Preset& p, const char* key, const char* value)
{
    for (int i = 0; i < p.count; ++i)
        if (!strcmp(p.values[i].key, key))
        {
            CHECK(optrules::SameValue(p.values[i].value, value));
            return;
        }
    CHECK(!"the bundle has this member");
}

void Bundles()
{
    const uimeta::PresetSet& hints = Set("button_hints");
    const uimeta::PresetSet& hits = Set("hit_effects");
    CHECK(hints.count == 3 && hits.count == 3);
    const char* const kKeys[] = {"prompt_bar", "world_prompts", "hud_context", "hud_prompts", "button_legend", "hud_glyphs"};
    const char* const kHints[3][6] = {{"0", "0", "0", "1", "0", "1"}, {"1", "0", "0", "0", "0", "1"}, {"2", "1", "1", "1", "1", "0"}};
    for (int w = 0; w < 3; ++w)
        for (int k = 0; k < 6; ++k)
            CheckMember(hints.presets[w], kKeys[k], kHints[w][k]);
    const char* const kHitKeys[] = {"shield_cue", "shield_cue_intensity", "hull_cue"};
    const char* const kHits[3][3] = {{"1", "1.0", "1"}, {"1", "0.5", "1"}, {"0", "1.0", "0"}};
    for (int w = 0; w < 3; ++w)
        for (int k = 0; k < 3; ++k)
            CheckMember(hits.presets[w], kHitKeys[k], kHits[w][k]);
    // each word reads itself, a member that differs reads CUSTOM (the row's last word), and the word stays one step away
    for (int w = 0; w < 3; ++w)
    {
        Values v = Apply(hints.presets[w]);
        CHECK(Word(hints, v, 3) == w);
        v["hud_prompts"] = v["hud_prompts"] == "0" ? "1" : "0";
        CHECK(Word(hints, v, 3) == 3);
        CHECK(Word(hints, Apply(hints.presets[w]), 3) == w);
    }
    Values soft = Apply(hits.presets[1]);
    CHECK(Word(hits, soft, 3) == 1);
    soft["shield_cue_intensity"] = "0.7";
    CHECK(Word(hits, soft, 3) == 3);
    soft["shield_cue_intensity"] = "0.500000"; // the registry's way of writing it
    CHECK(Word(hits, soft, 3) == 1);
    // a member no module owns does not decide: with the Controller off the HUD's two rows still name the word
    Values half = {{"hud_context", "1"}, {"hud_prompts", "1"}};
    CHECK(Word(hints, half, 3) == 2);
    CHECK(Word(hints, {}, 3) == 3);
    // a real row names its preset, only that one can match
    const uimeta::PresetSet& pads = Set("pad_preset");
    CHECK(Word(pads, Apply(pads.presets[0]), 3, 1) == 1); // FREELANCER's values on the row ACE: both hold, ACE reads
    CHECK(Word(pads, Apply(pads.presets[2]), 3, 0) == 3); // ASSISTED's values on the row FREELANCER: CUSTOM
    // the spec's rows: each preset row's last word is CUSTOM, and the bundles' defaults are their first word
    for (const char* key : {"button_hints", "hit_effects", "preset", "pad_preset"})
    {
        const uimeta::Row* r = uimeta::FindRow(key);
        CHECK(r && r->preset && !strcmp(r->options[r->optionCount - 1], "CUSTOM"));
    }
    CHECK(uimeta::FindRow("button_hints")->regKey == nullptr);
    CHECK(uimeta::FindRow("hit_effects")->regKey == nullptr);
}

void SpecShape()
{
    // five tabs by owner, HUD first; every tab but MORE opens on its head; a head's row name is a [Modules] row
    const char* const kIds[] = {"hud", "controller", "subtitles", "autopilot", "more"};
    CHECK(uimeta::kTabCount == 5);
    for (int t = 0; t < uimeta::kTabCount && t < 5; ++t)
        CHECK(!strcmp(uimeta::kTabs[t].id, kIds[t]));
    int heads = 0, simple = 0;
    for (const uimeta::Tab& t : uimeta::kTabs)
        for (int s = 0; s < t.count; ++s)
        {
            const uimeta::Section& sec = t.sections[s];
            CHECK(!sec.head || uimeta::FindRow(sec.head) != nullptr);
            for (int r = 0; r < sec.count; ++r)
            {
                const uimeta::Row& row = sec.rows[r];
                simple += row.tier == uimeta::Simple;
                if (!row.head)
                    continue;
                ++heads;
                CHECK(row.offHelp && *row.offHelp);
                bool known = false;
                for (const char* name : modules::kRowNames)
                    known = known || !strcmp(name, row.head);
                CHECK(known);
            }
        }
    CHECK(heads == 8);    // a head per module
    CHECK(simple == 39);  // 29 settings (0.4.6, issue 80: movie_intro removed), 2 + 6 heads, the page's rows, the changed count
    // the haptics' rows: Full is the default; trigger_effects is Player tier, the page shows it by pad (docs\UI_OPTIONS.md 4), so the count above stands
    const uimeta::Row* triggers = uimeta::FindRow("trigger_effects");
    CHECK(triggers && triggers->tier == uimeta::Player && triggers->optionCount == 3 && !strcmp(triggers->def, "2") && !strcmp(triggers->values[2], "0"));
    const uimeta::Row* mode = uimeta::FindRow("haptics_mode");
    CHECK(mode && mode->tier == uimeta::Player && !strcmp(mode->def, "0"));
    // the rows that write through a word list keep their registry values
    const uimeta::Row* style = uimeta::FindRow("hud_style");
    CHECK(style && style->values && !strcmp(style->values[0], "2") && !strcmp(style->def, "2"));
    const uimeta::Row* text = uimeta::FindRow("game_text_size");
    CHECK(text && text->optionCount == 4 && !strcmp(text->values[3], "150") && !strcmp(text->def, "0"));
    const uimeta::Row* zone = uimeta::FindRow("stick_deadzone");
    CHECK(zone && zone->scale == 100.0f && !strcmp(zone->def, "0.24"));
    CHECK(uimeta::kCardRowCount == 4);
    CHECK(uimeta::FindRegistryRow("Controller", "steer_expo")->kind == uimeta::Picker); // the number row steer_curve stands behind it
}

void FirstCardStamp()
{
    using settings_reset::FirstCard;
    using settings_reset::FirstCardFor;
    CHECK(FirstCardFor(false, false) == FirstCard::Show);
    CHECK(FirstCardFor(false, true) == FirstCard::Stamp);
    CHECK(FirstCardFor(true, false) == FirstCard::Nothing);
    CHECK(FirstCardFor(true, true) == FirstCard::Nothing);
    const std::string dir = g_test.temp + "\\first_card\\";
    CreateDirectoryA(dir.c_str(), nullptr);
    CHECK(!settings_reset::SettingsFound(dir.c_str()));
    CHECK(!settings_reset::CardStamped(dir.c_str()));
    settings_reset::StampCard(dir.c_str());
    CHECK(settings_reset::CardStamped(dir.c_str()));
    const std::string ini = dir + "FLController.ini";
    WritePrivateProfileStringA("Controller", "rumble", "100", ini.c_str());
    CHECK(settings_reset::SettingsFound(dir.c_str()));
}

void ButtonWords()
{
    using optrules::ExpandButtons;
    CHECK(ExpandButtons("Cycle: {Y} steps", optrules::Xbox, true, false) == "Cycle: Y steps");
    CHECK(ExpandButtons("Cycle: {Y} steps", optrules::PlayStation, true, false) == "Cycle: Triangle steps");
    CHECK(ExpandButtons("CYCLE ON {Y}", optrules::PlayStation, true, true) == "CYCLE ON TRIANGLE");
    CHECK(ExpandButtons("CYCLE ON {Y}", optrules::Nintendo, true, true) == "CYCLE ON X");
    CHECK(ExpandButtons("Press {A} to see", optrules::Xbox, false, false) == "Press Enter to see");
    CHECK(ExpandButtons("Press {A} to see", optrules::Nintendo, true, false) == "Press B to see");
    CHECK(ExpandButtons("{version} {Z} {", optrules::Xbox, true, false) == "{version} {Z} {");
}

void LayoutCard()
{
    for (int i = 0; i < static_cast<int>(presets::Action::COUNT); ++i)
        if (static_cast<presets::Action>(i) != presets::Action::None)
            CHECK(*presets::ActionWord(static_cast<presets::Action>(i)) != 0);
    for (int preset = presets::FREELANCER; preset <= presets::ASSISTED; ++preset)
    {
        const auto lines = presets::LayoutLines(presets::Effective(preset, 1));
        CHECK(lines.size() >= 8 && lines.size() <= 13); // one screen
    }
    const auto lines = presets::LayoutLines(presets::Effective(presets::FREELANCER, 1));
    bool rb = false, rs = false;
    for (const presets::LayoutLine& l : lines)
    {
        rb = rb || (l.btn == glyphs::RB && l.words == "Missile; hold: torpedo");
        rs = rs || (l.btn == glyphs::RS && l.words.find("Steer; click: ") == 0);
    }
    CHECK(rb && rs);
}
} // namespace

void Test_options_rules()
{
    HeadTable();
    RestartComparison();
    HeadWrites();
    ChangedRows();
    Bundles();
    SpecShape();
    FirstCardStamp();
    ButtonWords();
    LayoutCard();
}
