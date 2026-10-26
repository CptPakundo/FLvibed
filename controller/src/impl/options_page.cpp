// The FLVIBED OPTIONS page - see options_page.h (the PAGE track of the UI build, 25 Oct 2026).
//
// Rows come from core\ui_meta.h (the spec's tabs / sections / rows: kind, words, range, tier, needs, parent, help) and
// their values from the settings bus (core\settings_bus.h): no copy of any value lives here. A change goes to the bus
// at once (bus->Set: settings::Apply + the owner's `applied`); DEFAULT resets the tab's rows (bus->Reset); CANCEL puts
// every value back to the snapshot taken when the page opened; leaving saves every section with a change (bus->Save,
// the owner's ini writer). The preset rows (autopilot `preset`, controller `pad_preset`) apply their table and read
// CUSTOM when a member deviates. `show_advanced` lists every bus row the spec does not name, raw (label = key, help
// from the registry).
//
// Hosting: (a) inside the game's Options screen when the host switched to FLvibed's page (HostApi v18
// OptionsPageState() == 2: the host hid the game's page controls; this draws the page in the panel area with the
// game's measured geometry, FREELANCER_RE "Options screen"); (b) standalone as an overlay panel (the HUD panel look)
// from the pause menu's sixth button, the debug command `ui options` or Open(). Geometry = fractions of the screen
// height, so every resolution, aspect and DPI works as the subtitles' panels do.
#include "options_page.h"

#include "../fl.h"
#include "game/gamefont.h"
#include "game/menu.h"
#include "game/render.h"
#include "game/uidraw.h"
#include "host.h"
#include "log.h"
#include "version.h"
#include "padbases.h" // NoteFrame: the game's arrow on this pad screen
#include "promptbar.h"
#include "settings_bus.h"
#include "ui.h" // ControlRect: the Options screen's panel ("big box") while it slides
#include "hud2_kit.h" // ui_style HUD 2: the tabs, buttons and focus in HUD 2's kit (docs\UI_HUD2_EXTENDED.md 10)
#include "uiopt.h"
#include "uiskin.h"
#include "ui_meta.h"
#include "uikit.h"
#include "game/room.h" // uiclick::FindVisible
#include "flufaware.h"    // the FLUF row's note

#include <algorithm>
#include <fstream>
#include <sstream>

namespace options_page
{
using uikit::Widen;

// ------------------------------------------------------------------ state
enum class Host
{
    None,
    Options,   // inside the game's Options screen (the host's page switch)
    Standalone // the overlay panel
};

// One line of the current tab: a section header, a spec row, a raw registry row, or a bottom button.
struct Entry
{
    enum Kind
    {
        Header,
        Row,
        RawRow
    } kind = Row;
    const uimeta::Row* meta = nullptr; // Row: the spec's row
    const settings::Def* def = nullptr; // Row (with a registry row) / RawRow: the bus row
    std::string section, key;          // the bus section and registry key (Row: empty for a virtual row)
    std::wstring label;
    const char* help = "";
    bool interactive = false;
};

struct Layout
{
    float sw = 0, sh = 0, cx = 0;
    float left = 0, right = 0;       // the panel area (the frame the rows live in)
    float panelTop = 0, panelBottom = 0;
    float titleY = 0, titleX = 0;    // the title's centre
    float tabsY = 0, tabW = 0, tabPitch = 0, tabH = 0, tabX0 = 0;
    float rowsTop = 0, rowsBottom = 0, pitch = 0;
    float labelX = 0, controlLeft = 0, controlRight = 0;
    float buttonY = 0, buttonW = 0, buttonH = 0, defaultX = 0, cancelX = 0, closeX = 0;
    float previewTop = 0, previewH = 0; // the subtitles preview band (0 = none)
    float helpRight = 0, helpTop = 0, helpW = 0;
    int visible = 0; // rows that fit
};

struct Snapshot
{
    std::string section, key, value;
};

static Host g_hostMode = Host::None;
static float g_previewNeedH = 0; // the SUBTITLES preview band's height its sample needs (DrawPreview -> the next layout)
static bool g_open = false;
static int g_tab = 0;
static int g_focus = 0;  // index into g_entries; >= g_entries.size(): the bottom buttons (DEFAULT, CANCEL, CLOSE)
static int g_scroll = 0; // the first visible entry
static int g_hover = -1; // the entry under the pointer (-1 none), -2 .. the buttons / tabs
static std::vector<Entry> g_entries;
static std::vector<Snapshot> g_snapshot;
static bool g_capturing = false;   // a key row waits for a key
static uikit::Input g_input;
static Layout g_layout;
static bool g_dragging = false;    // the pointer drags a slider
// The scroll bar as Draw drew it (screen px), for the pointer: its arrows, track and thumb (the owner, 26 Oct 2026: the
// bars did not take the mouse; only the keys scrolled). Codes in HitTest: -20 / -21 the arrows, -22 / -23 the track above /
// below the thumb, -24 the thumb.
struct ScrollBar
{
    bool on = false;
    float x = 0, a = 0, upY = 0, downY = 0, trackTop = 0, trackH = 0, thumbY = 0, thumbH = 0;
};
static ScrollBar g_sb;
static int g_sbHeld = 0;         // the arrow / track part held down (0 none): steps repeat while held
static float g_sbRepeat = 0;     // seconds to its next step
static bool g_sbDrag = false;    // the pointer drags the thumb
static float g_sbGrab = 0;       // where on the thumb it was taken (px from its top)
static bool g_lastAdvanced = false; // show_advanced as the entries were last built (its confirmation)
static std::string g_toast;        // a line shown in the help area for a while (About, a refused key)
static float g_toastTime = 0;
static bool g_padDrove = false;    // the last input came from the pad (the pointer is ignored until it moves)
static float g_lastPx = -1, g_lastPy = -1;
static std::vector<std::string> g_pressQueue; // debug `ui press ...`
static bool g_suppressed = false;  // the pause menu's hit test is suppressed under the standalone panel
static u32 g_busGeneration = 0;
// The game's Options screen slides its parts in on every page switch (about 2 s: the page buttons from the left, the
// panel from the right, its own rows riding on the panel). The hosted page draws at the settled geometry, so during
// the slide it stood over the moving frame (release acceptance 25 Oct 2026 at 4:3: read as "the box narrower, the page's
// right third over its frame line" - the same at every aspect, a_options_flvibed_4x3_midslide_1s). The page waits:
// the host's page button rides the page buttons' frame (uibtn.cpp), so its rectangle - and the main menu's "big box"
// (ui::ControlRect: the panel itself) - standing still for kSettleMs says the screen has settled; kSettleCapMs in case
// the game never stands still.
static bool g_settled = false;
static float g_settleRect[7] = {};
static ULONGLONG g_settleChangedTick = 0, g_settleOpenTick = 0;
constexpr ULONGLONG kSettleMs = 150, kSettleCapMs = 3000;
static void FreePreview();
static void FreeCaption();

// MOVIES / Intro (`movie_intro`): only the files present are offered (UI_PLAN 3.3). The page checks the install's files
// itself (EXE\..\DATA\MOVIES, the names the subtitles logic maps the values to: movie.cpp ApplyIntro) when it opens:
// a missing file's word is greyed and marked, and stepping the picker skips it; the subtitles logic applies the same
// rule to a value set any other way (it resets a value naming a missing file to 0 and logs).
static const char* const kIntroFiles[] = {nullptr, "fl_intro.wmv", "fl_intro_vanilla.wmv", "fl_intro_full.wmv"};
static bool g_introHave[4] = {true, false, false, false};

static void RefreshIntroFiles()
{
    const std::string dir = std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "..\\DATA\\MOVIES\\";
    for (int i = 1; i < 4; ++i)
        g_introHave[i] = GetFileAttributesA((dir + kIntroFiles[i]).c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool IsIntroRow(const uimeta::Row* m) { return m && m->key && strcmp(m->key, "movie_intro") == 0; }
static bool IntroAvailable(int option) { return option >= 0 && option < 4 ? g_introHave[option] : false; }

// button pseudo-focus indices after the entries
enum
{
    kBtnDefault = 0,
    kBtnCancel = 1,
    kBtnClose = 2
};
static int ButtonCount() { return g_hostMode == Host::Standalone ? 3 : 2; }

// ------------------------------------------------------------------ the local bus (a host without one)
namespace localbus
{
struct Entry
{
    int id;
    SettingsBusSection sec;
    std::vector<std::string> defaults; // snapshotted at registration
};
static std::vector<Entry> g_sections;
static int g_nextId = 1;
static u32 g_generation = 0;

static Entry* FindSection(const char* section)
{
    for (Entry& e : g_sections)
        if (strcmp(e.sec.section, section) == 0)
            return &e;
    return nullptr;
}
static const settings::Def* FindDef(const char* section, const char* key, Entry** owner = nullptr)
{
    Entry* e = FindSection(section);
    if (owner)
        *owner = e;
    return e ? settings::Find(e->sec.defs, e->sec.count, key) : nullptr;
}
static int Register(const SettingsBusSection* s)
{
    if (!s || FindSection(s->section))
        return 0;
    Entry e;
    e.id = g_nextId++;
    e.sec = *s;
    for (size_t i = 0; i < s->count; ++i)
        e.defaults.push_back(s->defaults ? s->defaults[i] : settings::Value(s->defs[i]));
    g_sections.push_back(e);
    return e.id;
}
static void Unregister(int id)
{
    for (size_t i = 0; i < g_sections.size(); ++i)
        if (g_sections[i].id == id)
        {
            g_sections.erase(g_sections.begin() + i);
            return;
        }
}
static int Count() { return static_cast<int>(g_sections.size()); }
static const SettingsBusSection* Section(int i)
{
    return i >= 0 && i < Count() ? &g_sections[i].sec : nullptr;
}
static const settings::Def* Find(const char* section, const char* key) { return FindDef(section, key); }
static bool Default(const char* section, const char* key, char* out, int outLen)
{
    Entry* e = nullptr;
    const settings::Def* d = FindDef(section, key, &e);
    if (!d)
        return false;
    strncpy_s(out, outLen, e->defaults[d - e->sec.defs].c_str(), _TRUNCATE);
    return true;
}
static bool Set(const char* section, const char* key, const char* value)
{
    Entry* e = nullptr;
    const settings::Def* d = FindDef(section, key, &e);
    if (!d || !settings::Apply(*d, value))
        return false;
    ++g_generation;
    if (e->sec.applied)
        e->sec.applied(e->sec.ctx, d);
    return true;
}
static bool Get(const char* section, const char* key, char* out, int outLen)
{
    const settings::Def* d = FindDef(section, key);
    if (!d)
        return false;
    strncpy_s(out, outLen, settings::Value(*d).c_str(), _TRUNCATE);
    return true;
}
static bool Reset(const char* section, const char* key)
{
    char buf[128];
    return Default(section, key, buf, sizeof(buf)) && Set(section, key, buf);
}
static bool Save(const char* section)
{
    Entry* e = FindSection(section);
    if (!e || !e->sec.save)
        return false;
    e->sec.save(e->sec.ctx);
    return true;
}
static u32 Generation() { return g_generation; }
static const SettingsBus g_bus = {1, Register, Unregister, Count, Section, Find, Default, Set, Get, Reset, Save,
                                  Generation};
} // namespace localbus

const SettingsBus* LocalBus() { return &localbus::g_bus; }

static const SettingsBus* Bus()
{
    if (HostAtLeast(18) && g_host->GetSettingsBus)
        if (const SettingsBus* b = g_host->GetSettingsBus())
            return b;
    return localbus::Count() ? &localbus::g_bus : nullptr;
}

static std::string Get(const std::string& section, const std::string& key)
{
    char buf[128] = {};
    const SettingsBus* bus = Bus();
    if (bus && bus->Get(section.c_str(), key.c_str(), buf, sizeof(buf)))
        return buf;
    return "";
}

static std::string Default(const std::string& section, const std::string& key)
{
    char buf[128] = {};
    const SettingsBus* bus = Bus();
    if (bus && bus->Default(section.c_str(), key.c_str(), buf, sizeof(buf)))
        return buf;
    return "";
}

static bool Set(const std::string& section, const std::string& key, const std::string& value)
{
    const SettingsBus* bus = Bus();
    const bool ok = bus && bus->Set(section.c_str(), key.c_str(), value.c_str());
    if (!ok)
        Log("options page: set %s.%s = %s refused", section.c_str(), key.c_str(), value.c_str());
    return ok;
}

// Two registry values the same? Numbers compare as numbers ("1" == "1.0", "0.35" == "0.350000").
static bool SameValue(const std::string& a, const std::string& b)
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

static std::string FormatNumber(double v, int decimals)
{
    char buf[64];
    if (decimals <= 0)
        sprintf_s(buf, "%.0f", v);
    else
        sprintf_s(buf, "%.*f", decimals, v);
    return buf;
}

static int Decimals(float step)
{
    if (step <= 0 || step >= 1)
        return 0;
    int d = 0;
    double s = step;
    while (d < 4 && std::fabs(s - std::round(s)) > 1e-4) // (0.01f is 0.00999999977: compare rounded)
        s *= 10, ++d;
    return d;
}

// ------------------------------------------------------------------ the rows of a tab
static const char* TabRawSection(int tab)
{
    switch (tab)
    {
    case 0: return "Modules";
    case 1: return "Controller";
    case 2: return "Autopilot";
    case 3: return "Subtitles";
    }
    return nullptr;
}

static bool ShowAdvanced() { return atof(Get("Controller", "show_advanced").c_str()) != 0; }

static void BuildEntries()
{
    g_entries.clear();
    if (g_tab < 0 || g_tab >= uimeta::kTabCount)
        g_tab = 0;
    const uimeta::Tab& tab = uimeta::kTabs[g_tab];
    const bool advanced = ShowAdvanced();
    g_lastAdvanced = advanced;
    const SettingsBus* bus = Bus();
    for (int s = 0; s < tab.count; ++s)
    {
        const uimeta::Section& sec = tab.sections[s];
        bool any = false;
        for (int r = 0; r < sec.count; ++r)
            if (sec.rows[r].tier == uimeta::Player || advanced)
                any = true;
        if (!any)
            continue;
        if (sec.label && *sec.label)
        {
            Entry h;
            h.kind = Entry::Header;
            h.label = Widen(sec.label);
            g_entries.push_back(h);
        }
        for (int r = 0; r < sec.count; ++r)
        {
            const uimeta::Row& row = sec.rows[r];
            if (row.tier == uimeta::Advanced && !advanced)
                continue;
            Entry e;
            e.kind = Entry::Row;
            e.meta = &row;
            e.label = Widen(row.label);
            e.help = row.help;
            e.section = row.section;
            e.key = row.regKey ? row.regKey : "";
            e.def = (bus && row.regKey) ? bus->Find(row.section, row.regKey) : nullptr;
            // a row with a registry key the bus does not have (its module is off, or the row is not built yet): shown
            // greyed, not interactive, so the spec's rows are all visible and the gap is honest
            e.interactive = row.kind == uimeta::Button || !row.regKey || e.def != nullptr;
            g_entries.push_back(e);
        }
    }
    // show_advanced: every bus row of the tab's section the spec does not name, raw
    const char* raw = TabRawSection(g_tab);
    if (advanced && raw && bus)
    {
        bool header = false;
        for (int i = 0; i < bus->Count(); ++i)
        {
            const SettingsBusSection* sec = bus->Section(i);
            if (!sec || strcmp(sec->section, raw) != 0)
                continue;
            for (size_t r = 0; r < sec->count; ++r)
            {
                const settings::Def& d = sec->defs[r];
                if (uimeta::FindRegistryRow(sec->section, d.name))
                    continue;
                if (!header)
                {
                    Entry h;
                    h.kind = Entry::Header;
                    h.label = L"ADVANCED (" + Widen(raw) + L")";
                    g_entries.push_back(h);
                    header = true;
                }
                Entry e;
                e.kind = Entry::RawRow;
                e.def = &d;
                e.section = sec->section;
                e.key = d.name;
                e.label = Widen(d.name);
                e.help = d.help ? d.help : "";
                e.interactive = true;
                g_entries.push_back(e);
            }
        }
    }
}

static bool Interactive(int i)
{
    return i >= 0 && i < static_cast<int>(g_entries.size()) && g_entries[i].interactive;
}

// The parent row (the spec's `parent` key) is off: the row is greyed and inert.
static bool ParentOff(const Entry& e)
{
    if (e.kind != Entry::Row || !e.meta->parent)
        return false;
    const uimeta::Row* p = uimeta::FindRow(e.meta->parent);
    if (!p || !p->regKey)
        return false;
    return atof(Get(p->section, p->regKey).c_str()) == 0;
}

// ------------------------------------------------------------------ presets
static const uimeta::Preset* Presets(int tab, int& count)
{
    if (tab == 1)
    {
        count = uimeta::kControllerPresetCount;
        return uimeta::kControllerPresets;
    }
    if (tab == 2)
    {
        count = uimeta::kAutopilotPresetCount;
        return uimeta::kAutopilotPresets;
    }
    count = 0;
    return nullptr;
}

static bool PresetMatches(const uimeta::Preset& p)
{
    for (int i = 0; i < p.count; ++i)
        if (!SameValue(Get(p.values[i].section, p.values[i].key), p.values[i].value))
            return false;
    return true;
}

static void ApplyPreset(const uimeta::Preset& p)
{
    for (int i = 0; i < p.count; ++i)
        Set(p.values[i].section, p.values[i].key, p.values[i].value);
}

// The preset row's option index: the matching preset, else the last option (CUSTOM).
static int PresetIndex(const Entry& e)
{
    int n = 0;
    const uimeta::Preset* ps = Presets(g_tab, n);
    const int custom = e.meta->optionCount - 1;
    if (!ps)
        return custom;
    // a real row (pad_preset) names its preset; CUSTOM when its members deviate
    int chosen = -1;
    if (e.meta->regKey)
        chosen = atoi(Get(e.section, e.key).c_str());
    for (int i = 0; i < n && i < custom; ++i)
    {
        if (e.meta->regKey && chosen != i)
            continue;
        if (PresetMatches(ps[i]))
            return i;
    }
    return custom;
}

static void SetPreset(const Entry& e, int index)
{
    int n = 0;
    const uimeta::Preset* ps = Presets(g_tab, n);
    if (!ps || index < 0 || index >= n)
        return;
    if (e.meta->regKey)
        Set(e.section, e.key, std::to_string(index));
    ApplyPreset(ps[index]);
    Log("options page: preset %s", ps[index].name);
}

// ------------------------------------------------------------------ row values
// The picker's option index of a spec row (-1 = the registry holds a value no option names).
static int PickerIndex(const Entry& e)
{
    if (e.meta->preset)
        return PresetIndex(e);
    const std::string v = Get(e.section, e.key);
    if (e.meta->values)
    {
        for (int i = 0; i < e.meta->optionCount; ++i)
            if (SameValue(v, e.meta->values[i]))
                return i;
        return -1;
    }
    const int i = atoi(v.c_str());
    return i >= 0 && i < e.meta->optionCount ? i : -1;
}

static void SetPickerIndex(const Entry& e, int i)
{
    if (e.meta->preset)
    {
        int n = 0;
        Presets(g_tab, n);
        SetPreset(e, ((i % n) + n) % n);
        return;
    }
    const int n = e.meta->optionCount;
    i = ((i % n) + n) % n;
    Set(e.section, e.key, e.meta->values ? e.meta->values[i] : std::to_string(i));
}

static float SliderValue(const Entry& e) // in ui units
{
    return static_cast<float>(atof(Get(e.section, e.key).c_str())) * e.meta->scale;
}

static void SetSliderValue(const Entry& e, float v)
{
    const uimeta::Row& m = *e.meta;
    const float step = m.step > 0 ? m.step : 1.0f;
    v = std::round(v / step) * step;
    v = std::clamp(v, m.min, m.max);
    char buf[64];
    sprintf_s(buf, "%g", v / m.scale);
    Set(e.section, e.key, buf);
}

static std::wstring SliderCaption(const Entry& e, float v)
{
    const uimeta::Row& m = *e.meta;
    std::string s = FormatNumber(v, Decimals(m.step)) + m.unit;
    if (m.optionCount > 0 && m.max > m.min)
    {
        const int i = static_cast<int>(std::lround((v - m.min) / (m.max - m.min) * (m.optionCount - 1)));
        s += std::string("  ") + m.options[std::clamp(i, 0, m.optionCount - 1)];
    }
    return Widen(s.c_str());
}

// A raw registry row's step for left / right (an unclamped number steps by 1 / 10 %).
static double RawStep(const settings::Def& d, double v)
{
    if (d.type == settings::Int)
        return 1;
    if (d.min == 0 && d.max == 0)
        return std::max(0.01, std::fabs(v) * 0.1);
    if (d.max >= settings::kNoMax)
        return std::max(0.01, std::fabs(v) * 0.1);
    return (d.max - d.min) / 100.0;
}

static bool Changed(const Entry& e)
{
    if (e.key.empty())
        return false;
    return !SameValue(Get(e.section, e.key), Default(e.section, e.key));
}

static void Toast(const std::string& s)
{
    g_toast = s;
    g_toastTime = 4.0f;
}

// ------------------------------------------------------------------ open / close / save
static void TakeSnapshot()
{
    g_snapshot.clear();
    const SettingsBus* bus = Bus();
    if (!bus)
        return;
    for (int i = 0; i < bus->Count(); ++i)
    {
        const SettingsBusSection* sec = bus->Section(i);
        if (!sec)
            continue;
        for (size_t r = 0; r < sec->count; ++r)
            g_snapshot.push_back({sec->section, sec->defs[r].name, Get(sec->section, sec->defs[r].name)});
    }
}

// Leaving keeps: every section with a value changed since the snapshot is written through its owner.
static void SaveChanged()
{
    const SettingsBus* bus = Bus();
    if (!bus)
        return;
    std::vector<std::string> sections;
    for (const Snapshot& s : g_snapshot)
        if (!SameValue(Get(s.section, s.key), s.value) &&
            std::find(sections.begin(), sections.end(), s.section) == sections.end())
            sections.push_back(s.section);
    for (const std::string& s : sections)
        Log("options page: save %s -> %s", s.c_str(), bus->Save(s.c_str()) ? "ok" : "no writer");
}

static void RevertAll()
{
    int n = 0;
    for (const Snapshot& s : g_snapshot)
        if (!SameValue(Get(s.section, s.key), s.value))
            n += Set(s.section, s.key, s.value) ? 1 : 0;
    Log("options page: cancel reverted %d value(s)", n);
}

static void ResetTab()
{
    const SettingsBus* bus = Bus();
    if (!bus)
        return;
    int n = 0;
    for (const Entry& e : g_entries)
        if (!e.key.empty() && e.kind != Entry::Header)
            n += bus->Reset(e.section.c_str(), e.key.c_str()) ? 1 : 0;
    Log("options page: default reset %d row(s) of tab %s", n, uimeta::kTabs[g_tab].label);
}

static void ResetEverything()
{
    const SettingsBus* bus = Bus();
    if (!bus)
        return;
    int n = 0;
    for (int i = 0; i < bus->Count(); ++i)
    {
        const SettingsBusSection* sec = bus->Section(i);
        if (!sec)
            continue;
        for (size_t r = 0; r < sec->count; ++r)
            n += bus->Reset(sec->section, sec->defs[r].name) ? 1 : 0;
    }
    Log("options page: reset everything (%d rows)", n);
    Toast("Every FLvibed setting is back to its default");
}

static void SuppressPauseMenu(bool on)
{
    if (!HostAtLeast(4))
        return;
    if (on)
    {
        const uintptr_t root = menu::PauseMenu();
        if (!root)
            return;
        g_host->SetHudHitSuppression(true, &root, 1);
        g_suppressed = true;
    }
    else if (g_suppressed)
    {
        g_host->SetHudHitSuppression(false, nullptr, 0);
        g_suppressed = false;
    }
}

static void OpenAs(Host how)
{
    g_hostMode = how;
    g_open = true;
    g_focus = 0;
    g_scroll = 0;
    g_capturing = false;
    g_dragging = false;
    g_padDrove = false;
    g_lastPx = g_lastPy = -1;
    g_input.Reset();
    TakeSnapshot();
    RefreshIntroFiles();
    BuildEntries();
    while (g_focus < static_cast<int>(g_entries.size()) && !Interactive(g_focus))
        ++g_focus;
    if (how == Host::Standalone)
        SuppressPauseMenu(true);
    g_settled = how != Host::Options; // hosted: drawn once the screen's slide has settled (HostedSettled)
    g_settleOpenTick = g_settleChangedTick = GetTickCount64();
    memset(g_settleRect, 0, sizeof(g_settleRect));
    Log("options page: open (%s, %u rows on tab %s, %u values snapshotted)",
        how == Host::Options ? "options screen" : "standalone", static_cast<unsigned>(g_entries.size()),
        uimeta::kTabs[g_tab].label, static_cast<unsigned>(g_snapshot.size()));
}

static void CloseAs(bool keep)
{
    if (!g_open)
        return;
    if (keep)
        SaveChanged();
    g_open = false;
    g_capturing = false;
    SuppressPauseMenu(false);
    if (g_hostMode == Host::Options && HostAtLeast(18) && g_host->OptionsPageState &&
        g_host->OptionsPageState() == 2 && g_host->ShowOptionsPage)
        g_host->ShowOptionsPage(false); // the game's page back
    g_hostMode = Host::None;
    g_input.Reset();
    Log("options page: closed (%s)", keep ? "kept" : "reverted");
}

void Open()
{
    if (g_open)
        return;
    OpenAs(Host::Standalone);
}
void Close() { CloseAs(true); }
bool IsOpen() { return g_open; }
bool WantsInput() { return g_open; }

void Init() { g_input.Init(); }

void Shutdown()
{
    if (g_open)
        CloseAs(true);
    uikit::FreeFonts();
    FreePreview();
    FreeCaption();
}

// The hosted screen's slide has settled (see g_settled): once per frame from Draw.
static bool HostedSettled()
{
    if (g_settled)
        return true;
    float r[7] = {};
    const bool button = HostAtLeast(18) && g_host->UiButtonRect &&
                        g_host->UiButtonRect(kUiBtnOptionsPage, &r[0], &r[1], &r[2], &r[3]);
    ui::Item box;
    if (const u32 b = uiclick::FindVisible("big box"))
        if (ui::ControlRect(b, box))
            r[4] = box.x, r[5] = box.y, r[6] = box.depth;
    const ULONGLONG now = GetTickCount64();
    if (!button || memcmp(r, g_settleRect, sizeof(r)) != 0)
    {
        memcpy(g_settleRect, r, sizeof(r));
        g_settleChangedTick = now;
    }
    if (now - g_settleChangedTick >= kSettleMs || now - g_settleOpenTick >= kSettleCapMs)
    {
        g_settled = true;
        Log("options page: the screen settled after %u ms", static_cast<unsigned>(now - g_settleOpenTick));
    }
    return g_settled;
}

// ------------------------------------------------------------------ layout
static bool InGame() { return fl::PlayerIObj() != nullptr || fl::PlayerBase() != 0; }

// The player's text_size as the page draws it: the same clamp as HUD 2's (80..150 %).
static int TextPercent() { return std::clamp(uiopt::Get().textSize, 80, 150); }

static Layout ComputeLayout()
{
    Layout l;
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sw <= 0 || sh <= 0)
    {
        sw = fl::ScreenWidth();
        sh = fl::ScreenHeight();
    }
    l.sw = static_cast<float>(sw), l.sh = static_cast<float>(sh);
    l.cx = l.sw * 0.5f;
    const float H = l.sh;
    if (g_hostMode == Host::Options)
    {
        // the game's panel at 16:9 4K: x 1520..3240, the title at (2340, 346 / 432), rows from y ~600, pitch 98, the
        // buttons at y 1906 (main menu / in game: +86 / +27 px lower in game)
        const float ig = InGame() ? 1.0f : 0.0f;
        l.left = l.cx - 0.185f * H;
        l.right = l.cx + 0.611f * H;
        l.panelTop = 0.09f * H + ig * 0.04f * H;
        l.panelBottom = 0.95f * H;
        l.titleX = l.cx + 0.194f * H;
        l.titleY = (0.160f + ig * 0.040f) * H;
        l.tabsY = (0.252f + ig * 0.012f) * H;
        l.rowsTop = (0.292f + ig * 0.012f) * H;
        l.rowsBottom = 0.858f * H;
        l.buttonY = 0.882f * H;
        l.defaultX = l.left + 0.166f * H;
        l.cancelX = l.right - 0.221f * H;
        l.labelX = l.left + 0.046f * H;
        l.controlRight = l.right - 0.054f * H;
        l.helpRight = l.cx + 0.565f * H;
        l.helpTop = 0.006f * H;
    }
    else
    {
        const float halfW = std::min(0.45f * H, l.sw * 0.48f);
        l.left = l.cx - halfW;
        l.right = l.cx + halfW;
        l.panelTop = 0.08f * H;
        l.panelBottom = 0.90f * H;
        l.titleX = l.cx;
        l.titleY = 0.112f * H;
        l.tabsY = 0.170f * H;
        l.rowsTop = 0.215f * H;
        l.rowsBottom = 0.805f * H;
        l.buttonY = 0.848f * H;
        l.defaultX = l.cx - 0.27f * H;
        l.cancelX = l.cx;
        l.closeX = l.cx + 0.27f * H;
        l.labelX = l.left + 0.055f * H;
        l.controlRight = l.right - 0.06f * H;
        l.helpRight = l.right; // the rollover line above the panel, top right, as the game's sits at the screen's top
        l.helpTop = 0.006f * H;
    }
    l.helpW = 0.78f * H;
    l.controlLeft = l.controlRight - 0.36f * H;
    // text_size (FIX-TEXT 3, 25 Oct 2026: the page drew at 100 % whatever it said): the fonts follow it (TextPercent) and
    // the rows, tabs and buttons grow with them so a line keeps its height; the widths stay the Options screen's (the
    // page never leaves its area) - a word that no longer fits its box takes the fit rule (uikit / Hud2Caption), the
    // rows that no longer fit scroll. Below 100 % nothing shrinks: the label faces already stand at the legibility floor.
    const float t = std::max(1.0f, TextPercent() / 100.0f);
    l.pitch = 0.0454f * H * t;
    l.tabW = 0.128f * H;
    l.tabPitch = 0.1405f * H;
    l.tabH = std::min(0.038f * H * t, (l.rowsTop - l.tabsY) * 2 - 0.006f * H); // (never into the first row)
    l.tabX0 = l.labelX - 0.02f * H;
    l.buttonW = 0.185f * H;
    l.buttonH = 0.037f * H * t;
    l.buttonY -= (l.buttonH - 0.037f * H) * 0.5f; // (taller upward: the bottom edge stays inside the screen's frame)
    l.rowsBottom = std::min(l.rowsBottom, l.buttonY - l.buttonH * 0.5f - 0.004f * H);
    if (g_tab == 3) // SUBTITLES: the preview band at the bottom of the rows area (UI_PLAN 1.6), as tall as its sample needs
    {
        l.previewH = std::clamp(g_previewNeedH, 0.145f * H, 0.40f * H);
        l.previewTop = l.rowsBottom - l.previewH;
        l.rowsBottom = l.previewTop - 0.01f * H;
    }
    l.visible = std::max(1, static_cast<int>((l.rowsBottom - l.rowsTop) / l.pitch));
    return l;
}

static void KeepFocusVisible()
{
    const int n = static_cast<int>(g_entries.size());
    if (g_focus < n)
    {
        if (g_focus < g_scroll)
            g_scroll = g_focus;
        if (g_focus >= g_scroll + g_layout.visible)
            g_scroll = g_focus - g_layout.visible + 1;
        // a header directly above the focused row comes along
        if (g_focus - 1 >= 0 && g_entries[g_focus - 1].kind == Entry::Header && g_scroll == g_focus)
            g_scroll = g_focus - 1;
    }
    g_scroll = std::clamp(g_scroll, 0, std::max(0, n - g_layout.visible));
}

// The pointer's scrolling (bar, wheel): the rows move and the focus comes along into view (KeepFocusVisible would pull the
// list back to a focus left outside it): the first visible row when scrolled down past it, the last when up.
static void ScrollBy(int rows)
{
    const int n = static_cast<int>(g_entries.size());
    g_scroll = std::clamp(g_scroll + rows, 0, std::max(0, n - g_layout.visible));
    const int last = std::min(n, g_scroll + g_layout.visible) - 1;
    // a focus on the top row under a header that just scrolled out would pull the list back (KeepFocusVisible keeps that
    // header in view): the focus moves on to the next row instead
    if (g_focus < n && g_focus == g_scroll && g_focus > 0 && g_entries[g_focus - 1].kind == Entry::Header)
        for (int i = g_focus + 1; i <= last; ++i)
            if (Interactive(i))
            {
                g_focus = i;
                return;
            }
    if (g_focus >= n || (g_focus >= g_scroll && g_focus < g_scroll + g_layout.visible))
        return;
    if (g_focus < g_scroll)
    {
        for (int i = g_scroll; i <= last; ++i)
            if (Interactive(i))
            {
                g_focus = i;
                return;
            }
    }
    else
        for (int i = last; i >= g_scroll; --i)
            if (Interactive(i))
            {
                g_focus = i;
                return;
            }
}

static uikit::RowGeom GeomOf(int visibleIndex)
{
    uikit::RowGeom g;
    g.y = g_layout.rowsTop + visibleIndex * g_layout.pitch;
    g.h = g_layout.pitch;
    g.labelX = g_layout.labelX;
    g.controlLeft = g_layout.controlLeft;
    g.controlRight = g_layout.controlRight;
    g.sh = g_layout.sh;
    return g;
}

// ------------------------------------------------------------------ actions
static void MoveFocus(int dir)
{
    const int n = static_cast<int>(g_entries.size()), total = n + ButtonCount();
    int f = g_focus;
    for (int tries = 0; tries < total; ++tries)
    {
        f += dir;
        if (f < 0)
            f = total - 1;
        if (f >= total)
            f = 0;
        if (f >= n || Interactive(f))
            break;
    }
    g_focus = f;
    g_capturing = false;
    KeepFocusVisible();
}

static void SwitchTab(int tab)
{
    const int n = uimeta::kTabCount;
    g_tab = ((tab % n) + n) % n;
    g_focus = 0;
    g_scroll = 0;
    g_capturing = false;
    BuildEntries();
    while (g_focus < static_cast<int>(g_entries.size()) && !Interactive(g_focus))
        ++g_focus;
}

static void PressButton(int which)
{
    if (which == kBtnDefault)
        ResetTab();
    else if (which == kBtnCancel)
    {
        RevertAll();
        CloseAs(false); // hosted: the game's page comes back; standalone: the panel goes
    }
    else if (which == kBtnClose)
        CloseAs(true);
}

// left / right on a row (dir -1 / +1), accept (dir 0)
static void ActOnRow(Entry& e, int dir)
{
    if (!e.interactive || ParentOff(e))
        return;
    if (e.kind == Entry::RawRow)
    {
        const settings::Def& d = *e.def;
        if (d.type == settings::Bool)
        {
            const bool on = atof(Get(e.section, e.key).c_str()) != 0;
            Set(e.section, e.key, (dir == 0 ? !on : dir > 0) ? "1" : "0");
        }
        else if ((d.type == settings::Int || d.type == settings::Float) && dir != 0)
        {
            const double v = atof(Get(e.section, e.key).c_str());
            double nv = v + dir * RawStep(d, v);
            if (!(d.min == 0 && d.max == 0))
                nv = std::clamp(nv, static_cast<double>(d.min), static_cast<double>(d.max));
            char buf[64];
            sprintf_s(buf, d.type == settings::Int ? "%.0f" : "%g", nv);
            Set(e.section, e.key, buf);
        }
        return;
    }
    const uimeta::Row& m = *e.meta;
    switch (m.kind)
    {
    case uimeta::Toggle:
    {
        const bool on = atof(Get(e.section, e.key).c_str()) != 0;
        const bool want = dir == 0 ? !on : dir > 0;
        if (want != on)
            Set(e.section, e.key, want ? "1" : "0");
        break;
    }
    case uimeta::Picker:
    {
        const int i = PickerIndex(e);
        int next = i < 0 ? 0 : i + (dir == 0 ? 1 : dir);
        if (IsIntroRow(&m)) // Intro: the words of missing files are skipped (the available set, refreshed at open)
        {
            const int n = m.optionCount, step = dir < 0 ? -1 : 1;
            for (int tries = 0; tries < n && !IntroAvailable(((next % n) + n) % n); ++tries)
                next += step;
        }
        SetPickerIndex(e, next);
        break;
    }
    case uimeta::Slider:
        if (dir != 0)
            SetSliderValue(e, SliderValue(e) + dir * (m.step > 0 ? m.step : 1.0f));
        break;
    case uimeta::Key:
        if (dir == 0)
            g_capturing = !g_capturing;
        break;
    case uimeta::Button:
        if (dir == 0)
        {
            if (strcmp(m.key, "reset_all") == 0)
                ResetEverything();
            else if (strcmp(m.key, "about") == 0)
                Toast("FLvibed " FLV_VERSION " (built " __DATE__ ") - controller, subtitles and movies for Freelancer. "
                      "Freelancer (c) Microsoft; subtitle data GPL-3.0; credits in the README");
            else if (strcmp(m.key, "edit_bindings") == 0)
                Toast("The Bindings screen comes with the controller UI (phase D); the presets apply meanwhile");
        }
        break;
    }
}

static void CaptureKey(int vk)
{
    if (g_focus >= static_cast<int>(g_entries.size()))
        return;
    Entry& e = g_entries[g_focus];
    const std::string name = uikit::KeyName(vk);
    if (name.empty())
    {
        Toast("That key has no name here; try a letter, a function key or Insert / Delete / Home / End");
        return;
    }
    Set(e.section, e.key, name);
    g_capturing = false;
}

// ------------------------------------------------------------------ the pointer
// What is under the pointer: an entry index, or -2 - button, -10 - tab.
static int HitTest(float px, float py)
{
    const Layout& l = g_layout;
    const int n = static_cast<int>(g_entries.size());
    if (g_sb.on && std::fabs(px - g_sb.x) <= g_sb.a * 1.3f) // the scroll bar (before the rows: it may overlap them)
    {
        if (std::fabs(py - g_sb.upY) <= g_sb.a * 1.3f)
            return -20;
        if (std::fabs(py - g_sb.downY) <= g_sb.a * 1.3f)
            return -21;
        if (py >= g_sb.trackTop && py <= g_sb.trackTop + g_sb.trackH)
            return py < g_sb.thumbY ? -22 : py > g_sb.thumbY + g_sb.thumbH ? -23 : -24;
    }
    if (py >= l.rowsTop && py < l.rowsTop + l.visible * l.pitch && px >= l.left && px <= l.right)
    {
        const int i = g_scroll + static_cast<int>((py - l.rowsTop) / l.pitch);
        if (i >= 0 && i < n && Interactive(i))
            return i;
        return -1;
    }
    if (std::fabs(py - l.buttonY) <= l.buttonH * 0.5f)
    {
        const float xs[3] = {l.defaultX, l.cancelX, l.closeX};
        for (int b = 0; b < ButtonCount(); ++b)
            if (std::fabs(px - xs[b]) <= l.buttonW * 0.5f)
                return -2 - b;
    }
    if (std::fabs(py - l.tabsY) <= l.tabH * 0.5f)
        for (int t = 0; t < uimeta::kTabCount; ++t)
            if (px >= l.tabX0 + t * l.tabPitch && px <= l.tabX0 + t * l.tabPitch + l.tabW)
                return -10 - t;
    return -1;
}

static void ClickRow(Entry& e, int visibleIndex, float px, bool firstClick)
{
    if (!e.interactive || ParentOff(e))
        return;
    const uikit::RowGeom g = GeomOf(visibleIndex);
    if (e.kind == Entry::RawRow)
    {
        if (!firstClick)
            return;
        const uikit::PickerHit a = uikit::PickerArea(g);
        ActOnRow(e, e.def->type == settings::Bool ? 0 : (px < a.x + a.w * 0.5f ? -1 : 1));
        return;
    }
    switch (e.meta->kind)
    {
    case uimeta::Slider:
    {
        const uikit::SliderHit a = uikit::SliderArea(g);
        if (px >= a.x - g.sh * 0.02f && px <= a.x + a.w + g.sh * 0.02f)
        {
            const float t = std::clamp((px - a.x) / a.w, 0.0f, 1.0f);
            SetSliderValue(e, e.meta->min + t * (e.meta->max - e.meta->min));
            g_dragging = true;
        }
        break;
    }
    case uimeta::Picker:
        if (firstClick)
        {
            const uikit::PickerHit a = uikit::PickerArea(g);
            if (px >= a.x && px <= a.x + a.w)
                ActOnRow(e, px < a.x + (a.w - a.cell) * 0.5f ? -1 : 1);
            else
                ActOnRow(e, 1);
        }
        break;
    default:
        if (firstClick)
            ActOnRow(e, 0);
        break;
    }
}

// ------------------------------------------------------------------ per frame
static void HandleInput(const uikit::UiInput& in, float dt)
{
    const int n = static_cast<int>(g_entries.size());
    // the pad / keyboard
    if (in.anyPad || in.anyKey)
        g_padDrove = true;
    if (g_capturing && in.capturedVk)
    {
        CaptureKey(in.capturedVk);
        return;
    }
    if (in.back)
    {
        if (g_capturing)
            g_capturing = false;
        else if (g_hostMode == Host::Standalone)
            CloseAs(true);
        else
            CloseAs(true); // the Options screen: back to the game's page (the host keeps the screen open)
        return;
    }
    if (in.tabPrev)
        SwitchTab(g_tab - 1);
    if (in.tabNext)
        SwitchTab(g_tab + 1);
    if (in.up)
        MoveFocus(-1);
    if (in.down)
        MoveFocus(+1);
    if (in.pageUp)
    {
        g_focus = std::max(0, g_focus - g_layout.visible);
        while (g_focus < n && !Interactive(g_focus))
            ++g_focus;
        KeepFocusVisible();
    }
    if (in.pageDown)
    {
        g_focus = std::min(n - 1, g_focus + g_layout.visible);
        while (g_focus > 0 && !Interactive(g_focus))
            --g_focus;
        KeepFocusVisible();
    }
    if (g_focus < n)
    {
        if (in.left)
            ActOnRow(g_entries[g_focus], -1);
        if (in.right)
            ActOnRow(g_entries[g_focus], +1);
        if (in.accept)
            ActOnRow(g_entries[g_focus], 0);
    }
    else
    {
        // the bottom buttons: left / right move between them (they sit in one row; up / down keep cycling through
        // them as through the rows - release acceptance 25 Oct 2026)
        const int b = g_focus - n, count = ButtonCount();
        if (in.left && b > 0)
            g_focus = n + b - 1;
        if (in.right && b < count - 1)
            g_focus = n + b + 1;
        if (in.accept)
        {
            PressButton(b);
            return;
        }
    }
    if (!g_open)
        return;
    // the pointer: hover = focus once it moved since the pad drove; a click acts
    const bool moved = in.pointer && (in.px != g_lastPx || in.py != g_lastPy);
    if (moved)
        g_lastPx = in.px, g_lastPy = in.py, g_padDrove = false;
    g_hover = -1;
    if (in.pointer && !g_padDrove)
    {
        const int hit = HitTest(in.px, in.py);
        g_hover = hit;
        if (hit >= 0 && moved && !g_dragging && !g_sbDrag)
        {
            g_focus = hit;
            if (!g_capturing)
                KeepFocusVisible();
        }
        if (g_dragging && in.held && g_focus < n)
            ClickRow(g_entries[g_focus], g_focus - g_scroll, in.px, false);
        if (!in.held)
            g_dragging = false, g_sbDrag = false, g_sbHeld = 0;
        // the scroll bar: the thumb follows the pointer while dragged; a held arrow / track part repeats its step
        const int range = std::max(0, n - g_layout.visible);
        if (g_sbDrag && in.held && g_sb.on && range > 0 && g_sb.trackH > g_sb.thumbH)
        {
            const float t = std::clamp((in.py - g_sbGrab - g_sb.trackTop) / (g_sb.trackH - g_sb.thumbH), 0.0f, 1.0f);
            ScrollBy(static_cast<int>(std::lround(t * range)) - g_scroll);
        }
        auto step = [&](int part) { ScrollBy(part == -20 ? -1 : part == -21 ? 1 : part == -22 ? -g_layout.visible : g_layout.visible); };
        if (g_sbHeld && in.held && hit == g_sbHeld && (g_sbRepeat -= dt) <= 0)
            step(g_sbHeld), g_sbRepeat = 0.06f;
        if (in.click && hit <= -20 && hit >= -24)
        {
            if (hit == -24)
                g_sbDrag = true, g_sbGrab = in.py - g_sb.thumbY;
            else
                step(hit), g_sbHeld = hit, g_sbRepeat = 0.35f;
        }
        else if (in.click)
        {
            if (hit >= 0)
            {
                g_focus = hit;
                ClickRow(g_entries[hit], hit - g_scroll, in.px, true);
            }
            else if (hit <= -10)
                SwitchTab(-10 - hit);
            else if (hit <= -2)
                PressButton(-2 - hit);
            else if (g_capturing)
                g_capturing = false;
        }
    }
}

static void ApplyDebugPresses(uikit::UiInput& in)
{
    for (const std::string& p : g_pressQueue)
    {
        if (p == "up") in.up = true;
        else if (p == "down") in.down = true;
        else if (p == "left") in.left = true;
        else if (p == "right") in.right = true;
        else if (p == "a" || p == "accept") in.accept = true;
        else if (p == "b" || p == "back") in.back = true;
        else if (p == "lb") in.tabPrev = true;
        else if (p == "rb") in.tabNext = true;
        else if (p == "pgup") in.pageUp = true;
        else if (p == "pgdn") in.pageDown = true;
        in.anyKey = true;
    }
    g_pressQueue.clear();
}

void Update(double dtd)
{
    const float dt = std::clamp(static_cast<float>(dtd), 0.0f, 0.25f);
    // the host's page switch: the Options screen on FLvibed's page opens the hosted page, leaving it closes it
    const int state = HostAtLeast(18) && g_host->OptionsPageState ? g_host->OptionsPageState() : 0;
    if (state == 2 && !g_open)
        OpenAs(Host::Options);
    else if (state != 2 && g_open && g_hostMode == Host::Options)
        CloseAs(true);
    // the page button's state while the Options screen is open: shown, lit (the selected page's yellow) on our page
    if (state != 0 && HostAtLeast(18) && g_host->SetUiButton)
        g_host->SetUiButton(kUiBtnOptionsPage, true, state == 2, true);
    if (!g_open)
    {
        g_pressQueue.clear();
        if (HostAtLeast(27) && g_host->RealMouseWheel)
            g_host->RealMouseWheel(); // drained while closed: no old notches on the next open
        g_sb.on = false, g_sbDrag = false, g_sbHeld = 0;
        return;
    }
    if (g_toastTime > 0)
        g_toastTime -= dt;
    // a bus change from elsewhere (set / ap set / the ini): the entries' availability may change (show_advanced)
    const SettingsBus* bus = Bus();
    const u32 gen = bus ? bus->Generation() : 0;
    if (gen != g_busGeneration)
    {
        g_busGeneration = gen;
        const size_t was = g_entries.size();
        const bool wasAdvanced = g_lastAdvanced;
        BuildEntries();
        if (g_entries.size() != was)
            g_focus = std::min(g_focus, static_cast<int>(g_entries.size()) + ButtonCount() - 1);
        // its rows land on the other tabs, never on GENERAL where the switch is: say so (the owner, 26 Oct 2026:
        // "didn't appear to make a difference")
        if (g_lastAdvanced != wasAdvanced)
            Toast(g_lastAdvanced ? "Advanced settings shown: more rows on the CONTROLLER, AUTOPILOT and SUBTITLES tabs"
                                 : "Advanced settings hidden");
    }
    g_layout = ComputeLayout();
    KeepFocusVisible();
    uikit::UiInput in = g_input.Poll(dt, g_capturing);
    ApplyDebugPresses(in);
    HandleInput(in, dt);
    // the mouse wheel: 3 rows a notch (+ = up), the list's own as in the game's lists
    const int wheel = HostAtLeast(27) && g_host->RealMouseWheel ? g_host->RealMouseWheel() : 0;
    if (wheel && g_open && !g_capturing)
        ScrollBy(-wheel * 3);
    // the prompt pills (FIX2: every menu shows at least A and B; this page owns the pad while open, so padmenus' Options
    // prompts do not run here): a key row waiting for its key offers only B CANCEL
    if (g_open)
    {
        // the game's arrow follows the pad cursor rule here too (padbases decides; the pad's menu code does not run
        // while this page owns the pad - the arrow stood over the page's tabs; keyboard and mouse get it back there)
        padbases::NoteFrame(true);
        using promptbar::Prompt;
        if (g_capturing)
        {
            static const Prompt p[] = {{glyphs::B, L"CANCEL"}};
            promptbar::Set("options.flvibed.key", p, 1, true, 10);
        }
        else
        {
            static const Prompt p[] = {{glyphs::A, L"CHANGE"},
                                       {glyphs::DL, L"ADJUST", false, glyphs::DR},
                                       {glyphs::LB, L"TAB", false, glyphs::RB},
                                       {glyphs::B, L"BACK"}};
            promptbar::Set("options.flvibed", p, 4, false, 10);
        }
    }
}

// ------------------------------------------------------------------ the subtitles preview (UI_PLAN 1.6)
// A sample line drawn from the SAME settings the subtitles module reads (size words -> fraction of H, the faces, the
// text colour, the shadow, the background style / opacity, the speaker-name mode, the speaker colours), through the
// shared core\game\uidraw / gamefont code - the display's own drawing lives in the subtitles DLL, so this is a
// replica built from display.cpp's rules (kSizes, the Arial leading, the name font 1.2x bold, the paddings).
static render::Font g_pvText, g_pvName;
static int g_pvPx = 0, g_pvFace = -1;
static u32 g_juni = 0; // Juni's colour from EXE\FLSubtitles\speaker_colors.tsv (0 = not read)
static std::vector<std::wstring> Wrap(const render::Font& font, const std::wstring& s, float maxW, int maxLines);

static void FreePreview()
{
    render::FreeFont(g_pvText);
    render::FreeFont(g_pvName);
    g_pvPx = 0;
}

static u32 SampleSpeakerColor()
{
    if (g_juni)
        return g_juni;
    g_juni = 0xFF8DDB85; // the ally green until the table says otherwise
    std::ifstream in(std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLSubtitles\\speaker_colors.tsv");
    std::string l;
    while (std::getline(in, l))
    {
        if (l.rfind("Juni\t", 0) != 0)
            continue;
        std::vector<std::string> f;
        std::stringstream ss(l);
        std::string t;
        while (std::getline(ss, t, '\t'))
            f.push_back(t);
        if (f.size() > 4)
            g_juni = 0xFF000000 | static_cast<u32>(strtoul(f[4].c_str(), nullptr, 16));
        break;
    }
    return g_juni;
}

static void DrawPreview(const uikit::Fonts& f)
{
    const Layout& l = g_layout;
    if (l.previewH <= 0)
        return;
    const float sh = l.sh;
    // the band: a dark space-like backing with a label
    render::Box(l.left + sh * 0.02f, l.previewTop, l.right - l.left - sh * 0.04f, l.previewH, 0xC0050812);
    uidraw::Frame(l.left + sh * 0.02f, l.previewTop, l.right - l.left - sh * 0.04f, l.previewH,
                  std::max(1.0f, sh * 0.001f), uidraw::Fade(uidraw::kFrame, 0.5f));
    render::SetFont(f.body);
    render::Text(l.left + sh * 0.03f, l.previewTop + sh * 0.006f, L"PREVIEW", uidraw::Fade(uidraw::kText, 0.7f));
    const SettingsBus* bus = Bus();
    if (!bus || !bus->Find("Subtitles", "enabled") || atof(Get("Subtitles", "enabled").c_str()) == 0)
    {
        render::SetFont(f.label);
        const std::wstring off = bus && bus->Find("Subtitles", "enabled")
                                     ? L"SUBTITLES OFF"
                                     : L"SUBTITLES MODULE NOT CONNECTED (NO SETTINGS BUS IN THIS HOST)";
        render::Text(l.cx - render::TextWidth(f.label, off) * 0.5f, l.previewTop + l.previewH * 0.45f, off,
                     uidraw::kDisabled);
        return;
    }
    static const float kSizes[] = {0.030f, 0.036f, 0.044f, 0.054f, 0.072f};
    const int size = std::clamp(atoi(Get("Subtitles", "size").c_str()), 0, 4);
    const int face = atoi(Get("Subtitles", "font").c_str()) == 0 && gamefont::Load() ? 0 : 1;
    const int px = std::max(8, static_cast<int>(sh * kSizes[size] + 0.5f));
    if (px != g_pvPx || face != g_pvFace || !g_pvText.texture)
    {
        std::wstring chars;
        for (wchar_t c = 32; c < 127; ++c)
            chars += c;
        const bool text = face == 0 ? render::BuildFont(g_pvText, L"Arial Unicode MS", px, 400, chars)
                                    : render::BuildFont(g_pvText, L"Segoe UI", px, 600, chars);
        if (!text && !render::BuildFont(g_pvText, L"Segoe UI", px, 600, chars))
            return;
        if (!(face == 0 && render::BuildFont(g_pvName, L"Agency FB", static_cast<int>(px * 1.2f + 0.5f), 700, chars)) &&
            !render::BuildFont(g_pvName, L"Segoe UI", px, 700, chars))
            render::FreeFont(g_pvName);
        g_pvPx = px;
        g_pvFace = face;
    }
    const render::Font& nameFont = g_pvName.texture ? g_pvName : g_pvText;
    const bool arial = g_pvText.face == L"Arial Unicode MS";
    const float lineH = g_pvText.lineHeight * (arial ? 0.98f : 1.10f);
    const float padX = px * 0.35f, padY = (lineH - g_pvText.lineHeight) * 0.5f;
    const float nameH = nameFont.lineHeight * 1.02f;
    const int nameMode = std::clamp(atoi(Get("Subtitles", "speaker_names").c_str()), 0, 2);
    const bool colors = atof(Get("Subtitles", "speaker_colors").c_str()) != 0;
    const bool shadow = atof(Get("Subtitles", "shadow").c_str()) != 0;
    const int opacity = std::clamp(atoi(Get("Subtitles", "background_opacity").c_str()), 0, 100);
    const int style = atoi(Get("Subtitles", "background_style").c_str());
    const u32 textArgb = atoi(Get("Subtitles", "text_color").c_str()) == 1 ? 0xFFFFF04A : 0xFFFFFFFF;
    const u32 nameArgb = colors ? SampleSpeakerColor() : 0xFF8DDB85;
    const float shadowOff = std::max(1.0f, px / 16.0f);
    const std::wstring name = L"Juni";
    // the sample sentence wrapped as the module wraps (at most 42 characters a row: display.cpp kMaxChars), and to the
    // band's width when that is narrower (the module's rows may run wider on the full screen)
    const int avg = render::TextWidth(g_pvText, L"abcdefghijklmnopqrstuvwxyz ") / 27;
    const float bandW = l.right - l.left - sh * 0.04f;
    const float maxW = std::min(bandW - 2 * padX - sh * 0.04f, static_cast<float>(avg * 42));
    const std::wstring sample = L"Trent, this is Juni. The LSF needs a pilot it can trust - meet me at the bar.";
    const std::vector<std::wstring> rows = Wrap(g_pvText, nameMode == 1 ? name + L": " + sample : sample, maxW, 4);
    // the block: the name row (own row), then the text rows; inline: the name leads the first row
    std::vector<std::pair<std::wstring, bool>> lines; // text, isName
    if (nameMode == 0)
        lines.push_back({name, true});
    for (const std::wstring& r : rows)
        lines.push_back({r, false});
    float wmax = 0, h = 0;
    for (const auto& ln : lines)
    {
        const render::Font& fnt = ln.second ? nameFont : g_pvText;
        wmax = std::max(wmax, static_cast<float>(render::TextWidth(fnt, ln.first)));
        h += ln.second ? nameH : lineH;
    }
    g_previewNeedH = h + 2 * padY + sh * 0.04f; // the next layout sizes the band to it (the label and margins)
    // centred on the band (the hosted panel sits right of the screen's centre)
    const float cx = (l.left + l.right) * 0.5f, y0 = l.previewTop + (l.previewH - h) * 0.5f + sh * 0.008f;
    const float bx = cx - wmax * 0.5f - padX, by = y0 - padY, bw = wmax + 2 * padX, bh = h + 2 * padY;
    if (opacity > 0)
    {
        if (style == 1)
            uidraw::HudPanel(bx, by, bw, bh, opacity / 100.0f, sh, GetTickCount64() / 1000.0);
        else
            uidraw::SoftPanel(bx, by, bw, bh, opacity / 100.0f, std::max(2.0f, px * 0.12f));
    }
    float y = y0;
    for (const auto& ln : lines)
    {
        const render::Font& fnt = ln.second ? nameFont : g_pvText;
        render::SetFont(fnt);
        const float w = static_cast<float>(render::TextWidth(fnt, ln.first));
        const float x = cx - w * 0.5f;
        if (ln.second)
            uidraw::TextShadow(x, y, ln.first, nameArgb, shadow ? shadowOff : 0);
        else if (nameMode == 1 && ln.first.rfind(name + L": ", 0) == 0)
        {
            // inline: the name coloured, the text in the text colour
            const std::wstring head = name + L": ";
            const float hw = static_cast<float>(render::TextWidth(fnt, head));
            uidraw::TextShadow(x, y, head, nameArgb, shadow ? shadowOff : 0);
            uidraw::TextShadow(x + hw, y, ln.first.substr(head.size()), textArgb, shadow ? shadowOff : 0);
        }
        else
            uidraw::TextShadow(x, y, ln.first, textArgb, shadow ? shadowOff : 0);
        y += ln.second ? nameH : lineH;
    }
}

// ------------------------------------------------------------------ the page button's caption
// The FLVIBED caption of the Options screen's page button, drawn over the button's rectangle (HostApi v18 UiButtonRect)
// whenever the screen is open: the HOST created the button like the game's but could not make a native caption draw
// (uibtn.cpp kNativeOptionsCaption, 25 Oct 2026). The sibling captions' style, measured on options_general-003103.png
// at 4K: Agency FB Regular with the caps 56 px tall ("GENERAL" 188 px wide, "MAIN MENU" 239) = 72 px em, 0.0333 H;
// centred on the button; the normal light blue (uidraw::kText), the selected page's yellow while FLvibed's page shows
// (OptionsPageState 2); the game's dark shadow down-right.
static render::Font g_capFont;

static void FreeCaption() { render::FreeFont(g_capFont); }

static void DrawCaption()
{
    if (!HostAtLeast(18) || !g_host->UiButtonRect || !g_host->OptionsPageState || !g_host->GetD3DDevice)
        return;
    const int state = g_host->OptionsPageState();
    if (state == 0)
        return;
    float cx = 0, cy = 0, hw = 0, hh = 0;
    if (!g_host->UiButtonRect(kUiBtnOptionsPage, &cx, &cy, &hw, &hh) || hh <= 1)
        return;
    void* device = g_host->GetD3DDevice();
    if (!device)
        return;
    render::SetDevice(device); // (in menus nothing else sets it: ScreenSize needs it)
    int sw = 0, sh = 0;
    if (!render::ScreenSize(sw, sh) || sh < 100)
        return;
    // (+1.5 px: the game's captions come out 56 px tall at 2160 and 20 px at 720 - its rasteriser rounds up at small
    // sizes; 72 / 24 px from the fraction alone gave 55 / 18, 73 / 26 give 56 / 20)
    const int px = std::max(8, static_cast<int>(sh * 0.0333f + 1.5f));
    if (g_capFont.px != px || !g_capFont.texture)
    {
        const bool game = gamefont::Load();
        if (!render::BuildFont(g_capFont, game ? gamefont::LabelFace() : L"Segoe UI", px, game ? 400 : 600,
                               L"ABCDEFGHIJKLMNOPQRSTUVWXYZ "))
            return;
    }
    if (!render::Begin(g_capFont))
        return;
    const std::wstring cap = L"FLVIBED";
    const float w = static_cast<float>(render::TextWidth(g_capFont, cap));
    // the capitals centred on the button (the game centres its captions' caps on the button): the 'F' glyph's box
    float capTop = g_capFont.ascent * 0.25f, capH = g_capFont.ascent * 0.75f;
    const auto it = g_capFont.glyphs.find(L'F');
    if (it != g_capFont.glyphs.end() && it->second.h > 0)
        capTop = static_cast<float>(it->second.oy), capH = static_cast<float>(it->second.h);
    const float x = cx - w * 0.5f, y = cy - capTop - capH * 0.5f;
    // the sibling captions as the game draws them: (132, 199, 231) normal (measured on CONTROLS / MAIN MENU), the
    // selected (247, 236, 65) ~ uidraw::kSelected
    constexpr u32 kCaptionNormal = 0xFF84C7E7;
    uidraw::TextShadow(x, y, cap, state == 2 ? uidraw::kSelected : kCaptionNormal, std::max(1.0f, sh * 0.0012f));
    render::End(false);
}

// ------------------------------------------------------------------ drawing
static std::vector<std::wstring> Wrap(const render::Font& font, const std::wstring& s, float maxW, int maxLines)
{
    std::vector<std::wstring> out;
    std::wstring line, word;
    auto flush = [&]()
    {
        if (!line.empty())
            out.push_back(line);
        line.clear();
    };
    for (size_t i = 0; i <= s.size(); ++i)
    {
        const wchar_t c = i < s.size() ? s[i] : L' ';
        if (c != L' ')
        {
            word += c;
            continue;
        }
        if (word.empty())
            continue;
        const std::wstring cand = line.empty() ? word : line + L" " + word;
        if (render::TextWidth(font, cand) > maxW && !line.empty())
        {
            flush();
            line = word;
        }
        else
            line = cand;
        word.clear();
    }
    flush();
    if (static_cast<int>(out.size()) > maxLines)
    {
        out.resize(maxLines);
        out.back() += L"\x2026";
    }
    return out;
}

static void DrawHelp(const uikit::Fonts& f)
{
    const Layout& l = g_layout;
    std::wstring text;
    if (g_toastTime > 0)
        text = Widen(g_toast.c_str());
    else if (g_hover >= 0 && g_hover < static_cast<int>(g_entries.size()))
        text = Widen(g_entries[g_hover].help);
    else if (g_focus < static_cast<int>(g_entries.size()))
        text = Widen(g_entries[g_focus].help);
    else
    {
        const int b = g_focus - static_cast<int>(g_entries.size());
        if (b == kBtnDefault)
            text = L"This tab's rows back to their defaults.";
        else if (b == kBtnCancel)
            text = L"Every change since the page opened is undone.";
        else
            text = L"Keeps the changes and closes the page.";
    }
    if (text.empty())
        return;
    render::SetFont(f.body);
    const std::vector<std::wstring> lines = Wrap(f.body, text, l.helpW, 3);
    float w = 0;
    for (const std::wstring& ln : lines)
        w = std::max(w, static_cast<float>(render::TextWidth(f.body, ln)));
    const float pad = l.sh * 0.004f, lh = f.body.lineHeight * 1.05f;
    const float x = l.helpRight - w - pad, y = l.helpTop;
    render::Box(x - pad, y, w + 2 * pad, lines.size() * lh + 2 * pad, uidraw::kHelpBack);
    for (size_t i = 0; i < lines.size(); ++i)
        render::Text(l.helpRight - render::TextWidth(f.body, lines[i]) - pad, y + pad + i * lh, lines[i],
                     uidraw::kText);
}

static void DrawEntry(const uikit::Fonts& f, Entry& e, int visibleIndex, bool focused)
{
    const uikit::RowGeom g = GeomOf(visibleIndex);
    if (e.kind == Entry::Header)
    {
        render::SetFont(f.title);
        uidraw::TextShadow(g.labelX - g.sh * 0.037f, g.y + (g.h - f.title.lineHeight) * 0.5f, e.label, uidraw::kText,
                           std::max(1.0f, g.sh * 0.001f));
        return;
    }
    uikit::RowStyle s;
    s.focused = focused && !uiskin::On(); // HUD 2: the one focus style is drawn by DrawHud2Back
    s.enabled = e.interactive && !ParentOff(e);
    s.changed = Changed(e);
    if (e.kind == Entry::Row)
    {
        if (e.meta->needs == uimeta::Restart)
            s.note = L"AFTER RESTART";
        else if (e.meta->needs == uimeta::Load)
            s.note = L"AT THE NEXT LOAD";
        // the FLUF row says whether FLUF is there and whether FLvibed reads its windows (docs\MOD_COMPATIBILITY.md FLUF)
        if (e.meta->key && strcmp(e.meta->key, "fluf_windows") == 0)
            s.note = !flufaware::Present()               ? L"NO FLUF INSTALLED"
                     : flufaware::State().present == 2   ? L"FLUF DETECTED"
                                                         : L"FLUF DETECTED - CO-RESIDENCY ONLY";
        // the borderless row says who provides it at this start (the host's winmode.cpp; the setting acts at the next)
        if (e.meta->key && strcmp(e.meta->key, "borderless") == 0)
        {
            const char* p = HostWindowMode(nullptr, 0);
            s.note = !strcmp(p, "flvibed")      ? L"BY FLVIBED"
                     : !strcmp(p, "game")       ? L"BY THE GAME (HDE)"
                     : !strcmp(p, "wrapper")    ? L"BY THE DIRECT3D WRAPPER"
                     : !strcmp(p, "unverified") ? L"NOT AVAILABLE - THE GAME'S MODE"
                     : !strcmp(p, "off")        ? L"OFF"
                                                : L"AFTER RESTART";
        }
    }
    uikit::RowLabel(f, g, s, e.label);
    const std::string v = e.key.empty() ? "" : Get(e.section, e.key);
    if (e.kind == Entry::RawRow)
    {
        const settings::Def& d = *e.def;
        if (d.type == settings::Bool)
            uikit::RowToggle(f, g, s, atof(v.c_str()) != 0);
        else
            uikit::RowPicker(f, g, s, Widen(v.c_str()));
        return;
    }
    const uimeta::Row& m = *e.meta;
    switch (m.kind)
    {
    case uimeta::Toggle: uikit::RowToggle(f, g, s, atof(v.c_str()) != 0); break;
    case uimeta::Picker:
    {
        const int i = PickerIndex(e);
        std::wstring word;
        if (!e.interactive)
            word = L"NOT AVAILABLE";
        else if (i >= 0)
            word = Widen(m.options[i]);
        else
            word = Widen(v.c_str());
        if (IsIntroRow(&m) && i >= 0 && !IntroAvailable(i)) // a file this install lacks: the word greyed and marked
        {
            uikit::RowStyle grey = s;
            grey.enabled = false;
            uikit::RowPicker(f, g, grey, word + L" (MISSING)");
            break;
        }
        uikit::RowPicker(f, g, s, word);
        break;
    }
    case uimeta::Slider:
    {
        const float val = SliderValue(e);
        const float t = m.max > m.min ? (val - m.min) / (m.max - m.min) : 0;
        uikit::RowSlider(f, g, s, t, SliderCaption(e, val));
        break;
    }
    case uimeta::Key: uikit::RowKey(f, g, s, Widen(v.c_str()), focused && g_capturing); break;
    case uimeta::Button:
    {
        const float bw = g.sh * 0.335f, bh = g.h * 0.72f;
        uikit::Button(f, g.controlRight - bw, g.y + (g.h - bh) * 0.5f, bw, bh, g.sh, Widen(m.btn ? m.btn : "..."),
                      focused, true);
        break;
    }
    }
}

// ui_style HUD 2 (docs\UI_HUD2_EXTENDED.md 10; the mock tools\uimock\hud2x.py `tab` / `focus`): the page's chrome in
// HUD 2's kit, under the texts the classic pass draws after it - the tab and button plates (no purple fill; the
// current tab with the yellow underline), the focused row's fill at 28 % active yellow with the bar outside its left
// edge, the standalone panel as one glass plate.
static void DrawHud2Back(const Layout& l, int n, int focusBtn)
{
    void* device = g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr;
    int w = 0, h = 0;
    if (!device || !render::ScreenSize(w, h) || h < 100)
        return;
    const uiopt::UiOptions& o = uiopt::Get();
    const float k = std::min(1.3f, std::max(0.8f, o.hudScale / 100.0f)) * std::min(1.5f, std::max(0.8f, o.textSize / 100.0f));
    if (!hud2kit::Begin(device, w, h, std::max(8, static_cast<int>(0.026f * h * k + 0.5f))))
        return;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    const float H = l.sh, c = H * 0.010f;
    const int tlbr = hud2kit::kTL | hud2kit::kBR;
    // the standalone panel fully opaque (fix-text 3: at 0.85 the room behind - a character's name label "Trent" - showed
    // through the rows); hosted, the Options screen's own window is the backing
    if (g_hostMode == Host::Standalone)
        hud2kit::Plate(l.left - H * 0.02f, l.panelTop, l.right + H * 0.02f, l.panelBottom, H * 0.012f, tlbr, 1.0f,
                       hud2kit::kChrome, 175 / 255.0f);
    auto tab = [&](float x, float y, float tw, float th, bool current, bool lit)
    {
        hud2kit::Plate(x, y, x + tw, y + th, c, tlbr, 220 / 255.0f, hud2kit::kChrome, (current || lit ? 200 : 120) / 255.0f);
        if (current)
            hud2kit::Line(x + H * 0.012f, y + th - H * 0.004f, x + tw - H * 0.012f, y + th - H * 0.004f, H * 0.004f,
                          hud2kit::Argb(hud2kit::kActive, 1));
    };
    for (int t = 0; t < uimeta::kTabCount; ++t)
        tab(l.tabX0 + t * l.tabPitch, l.tabsY - l.tabH * 0.5f, l.tabW, l.tabH, t == g_tab, g_hover == -10 - t);
    const float by = l.buttonY - l.buttonH * 0.5f;
    tab(l.defaultX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, false, g_hover == -2 - kBtnDefault);
    tab(l.cancelX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, false, g_hover == -2 - kBtnCancel);
    if (g_hostMode == Host::Standalone)
        tab(l.closeX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, false, g_hover == -2 - kBtnClose);
    // the focus: a row (its label to the control's right end, as the classic frame) or a bottom button
    float fx0 = 0, fy0 = 0, fx1 = 0, fy1 = 0;
    if (g_focus >= 0 && g_focus < n && g_focus >= g_scroll && g_focus - g_scroll < l.visible &&
        g_entries[g_focus].kind != Entry::Header)
    {
        const uikit::RowGeom g = GeomOf(g_focus - g_scroll);
        fx0 = g.labelX - H * 0.03f, fy0 = g.y, fx1 = g.controlRight + H * 0.015f, fy1 = g.y + g.h;
    }
    else if (focusBtn >= 0)
    {
        const float cx = focusBtn == kBtnDefault ? l.defaultX : focusBtn == kBtnCancel ? l.cancelX : l.closeX;
        fx0 = cx - l.buttonW * 0.5f, fy0 = by, fx1 = cx + l.buttonW * 0.5f, fy1 = by + l.buttonH;
    }
    if (fx1 > fx0)
    {
        hud2kit::Rect(fx0, fy0, fx1 - fx0, fy1 - fy0, hud2kit::Argb(hud2kit::kActive, 0.28f));
        hud2kit::Rect(fx0 - H * 0.013f, fy0, H * 0.005f, fy1 - fy0, hud2kit::Argb(hud2kit::kActive, 1));
    }
    hud2kit::End();
}

// Hosted, the page's backing is the Options screen's own window - translucent in both looks (the game's art; HUD 2's
// glass at 0.85): the room behind showed through the rows (fix-text 3). Under the UI pass, before the skin's glass and
// the game's art, an opaque plate fills the window's frame (the skin's measured plate for that window, any ui_style), so
// the window keeps its look over a solid base. Standalone, the page's own panel is opaque (Draw / DrawHud2Back).
void RenderUnder(void* device)
{
    if (!g_open || g_hostMode != Host::Options || !g_settled || !device)
        return;
    const char* anchor = uiclick::FindVisible("OptionsBox") ? "OptionsBox" : "big box"; // in game / the main menu
    const u32 box = uiclick::FindVisible(anchor);
    float off[4];
    ui::Item it;
    int w = 0, h = 0;
    if (!box || !uiskin::PlateOffsets("options", anchor, off) || !ui::ControlRect(box, it) || !render::ScreenSize(w, h) ||
        h < 100)
        return;
    const float H = static_cast<float>(h);
    if (!hud2kit::Begin(device, w, h, std::max(8, static_cast<int>(0.026f * H + 0.5f))))
        return;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    hud2kit::Plate(it.x + off[0] * H, it.y + off[1] * H, it.x + off[2] * H, it.y + off[3] * H, H * 0.012f,
                   hud2kit::kTL | hud2kit::kBR, 1.0f, hud2kit::kChrome, 0);
    hud2kit::End();
}

// A caption centred in a box (the HUD 2 form of uikit's TabButton / Button: the plate is DrawHud2Back's).
static void Hud2Caption(const uikit::Fonts& f, float x, float y, float w, float h, float sh, const std::wstring& caption,
                        bool selected)
{
    render::SetFont(f.label);
    const textfit::Result c = uidraw::FitText(f.label, caption, w - sh * 0.01f, textfit::MinSize(sh)); // the fit rule
    uidraw::TextShadowFit(x + (w - c.width) * 0.5f, y + (h - f.label.lineHeight) * 0.5f, f.label, c,
                          selected ? uidraw::kSelected : uidraw::kText, std::max(1.0f, sh * 0.001f));
}

void Draw()
{
    DrawCaption(); // the page button's caption whenever the Options screen is open (the page itself only when open)
    if (!g_open || !HostedSettled())
        return;
    if (g_layout.sh <= 0)
        g_layout = ComputeLayout();
    const Layout& l = g_layout;
    if (g_host && g_host->GetD3DDevice)
        render::SetDevice(g_host->GetD3DDevice()); // the renderer's device (in menus nothing else sets it)
    const uikit::Fonts& f = uikit::FontsFor(static_cast<int>(l.sh), TextPercent());
    const bool hud2 = uiskin::On();
    const int nAll = static_cast<int>(g_entries.size());
    if (hud2 && f.ok)
        DrawHud2Back(l, nAll, g_focus >= nAll ? g_focus - nAll : -1);
    if (!f.ok || !render::Begin(f.label))
    {
        static u32 warned = 0;
        if (warned++ == 0)
            Log("options page: nothing drawn (fonts %d, device %p)", f.ok ? 1 : 0,
                g_host && g_host->GetD3DDevice ? g_host->GetD3DDevice() : nullptr);
        return;
    }
    const float sh = l.sh;
    const double now = GetTickCount64() / 1000.0;
    if (g_hostMode == Host::Standalone && !hud2)
        uidraw::HudPanel(l.left - sh * 0.02f, l.panelTop, l.right - l.left + sh * 0.04f, l.panelBottom - l.panelTop,
                         1.0f, sh, now); // opaque: nothing of the room behind shows through (fix-text 3)
    // the title
    render::SetFont(f.title);
    const std::wstring title = L"FLVIBED OPTIONS";
    uidraw::TextShadow(l.titleX - render::TextWidth(f.title, title) * 0.5f, l.titleY - f.title.lineHeight * 0.5f,
                       title, uidraw::kText, std::max(1.0f, sh * 0.001f));
    // the tabs
    for (int t = 0; t < uimeta::kTabCount; ++t)
        if (hud2)
            Hud2Caption(f, l.tabX0 + t * l.tabPitch, l.tabsY - l.tabH * 0.5f, l.tabW, l.tabH, sh,
                        Widen(uimeta::kTabs[t].label), t == g_tab);
        else
            uikit::TabButton(f, l.tabX0 + t * l.tabPitch, l.tabsY - l.tabH * 0.5f, l.tabW, l.tabH, sh,
                             Widen(uimeta::kTabs[t].label), t == g_tab, g_hover == -10 - t);
    // the rows
    const int n = static_cast<int>(g_entries.size());
    for (int i = g_scroll, vi = 0; i < n && vi < l.visible; ++i, ++vi)
        DrawEntry(f, g_entries[i], vi, i == g_focus);
    g_sb.on = n > l.visible;
    if (n > l.visible) // the scroll arrows at the right, as the Controls page's table has them
    {
        const float ax = l.controlRight + sh * 0.028f, a = sh * 0.012f;
        uidraw::Triangle(ax, l.rowsTop + a, a, 0, g_scroll > 0 ? uidraw::kText : uidraw::kDisabled);
        uidraw::Triangle(ax, l.rowsTop + l.visible * l.pitch - a, a, 1,
                         g_scroll + l.visible < n ? uidraw::kText : uidraw::kDisabled);
        // the thumb between them
        const float trackTop = l.rowsTop + 2.5f * a, trackH = l.visible * l.pitch - 5.0f * a;
        const float th = std::max(a, trackH * l.visible / n), ty = trackTop + (trackH - th) * g_scroll / std::max(1, n - l.visible);
        g_sb.x = ax, g_sb.a = a, g_sb.upY = l.rowsTop + a, g_sb.downY = l.rowsTop + l.visible * l.pitch - a;
        g_sb.trackTop = trackTop, g_sb.trackH = trackH, g_sb.thumbY = ty, g_sb.thumbH = th;
        render::Box(ax - a * 0.25f, trackTop, a * 0.5f, trackH, uidraw::Fade(uidraw::kTrack, 0.4f));
        render::Box(ax - a * 0.4f, ty, a * 0.8f, th, uidraw::kThumb);
    }
    if (g_tab == 3)
        DrawPreview(f);
    // the bottom buttons
    const float by = l.buttonY - l.buttonH * 0.5f;
    const int focusBtn = g_focus >= n ? g_focus - n : -1;
    if (hud2)
    {
        Hud2Caption(f, l.defaultX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"DEFAULT", false);
        Hud2Caption(f, l.cancelX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"CANCEL", false);
        if (g_hostMode == Host::Standalone)
            Hud2Caption(f, l.closeX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"CLOSE", false);
        DrawHelp(f);
        render::End(false);
        return;
    }
    uikit::Button(f, l.defaultX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"DEFAULT",
                  focusBtn == kBtnDefault || g_hover == -2 - kBtnDefault, true);
    uikit::Button(f, l.cancelX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"CANCEL",
                  focusBtn == kBtnCancel || g_hover == -2 - kBtnCancel, true);
    if (g_hostMode == Host::Standalone)
        uikit::Button(f, l.closeX - l.buttonW * 0.5f, by, l.buttonW, l.buttonH, sh, L"CLOSE",
                      focusBtn == kBtnClose || g_hover == -2 - kBtnClose, true);
    DrawHelp(f);
    render::End(false);
}

// ------------------------------------------------------------------ host events
void OnUiButton(int id)
{
    if (id == kUiBtnPauseFlvibed)
    {
        if (!g_open)
            OpenAs(Host::Standalone);
        return;
    }
    if (id == kUiBtnOptionsPage)
    {
        // the host switches the page itself (the state byte); Update sees state 2 and opens the hosted page
        if (HostAtLeast(18) && g_host->OptionsPageState && g_host->OptionsPageState() == 1 && g_host->ShowOptionsPage)
            g_host->ShowOptionsPage(true);
    }
}

bool OnRawKey(int vk, bool down, bool textField)
{
    if (!g_open || textField)
        return false;
    if (g_hostMode == Host::Options && vk == VK_ESCAPE && !g_capturing)
        return false; // the game's own close of the Options screen (the host restores its page; Update closes ours)
    return g_input.PushKey(vk, down, g_capturing);
}

// ------------------------------------------------------------------ debug
static std::string RowsReport()
{
    std::ostringstream o;
    o << "tab " << g_tab << " " << uimeta::kTabs[g_tab].label << " rows=" << g_entries.size() << " focus=" << g_focus
      << " scroll=" << g_scroll << " visible=" << g_layout.visible << " bar=" << (g_sb.on ? 1 : 0) << " x="
      << static_cast<int>(g_sb.x) << " up=" << static_cast<int>(g_sb.upY) << " down=" << static_cast<int>(g_sb.downY)
      << " thumb=" << static_cast<int>(g_sb.thumbY) << "+" << static_cast<int>(g_sb.thumbH) << "\n";
    for (size_t i = 0; i < g_entries.size(); ++i)
    {
        Entry& e = g_entries[i];
        o << (static_cast<int>(i) == g_focus ? "> " : "  ");
        if (e.kind == Entry::Header)
        {
            o << "[" << uikit::Narrow(e.label) << "]\n";
            continue;
        }
        o << (e.kind == Entry::RawRow ? "raw " : "") << (e.kind == Entry::Row ? e.meta->key : e.key);
        if (!e.key.empty())
        {
            o << " = " << Get(e.section, e.key);
            if (e.kind == Entry::Row && e.meta->kind == uimeta::Picker)
            {
                const int pi = PickerIndex(e);
                o << " (" << (pi >= 0 ? e.meta->options[pi] : "?") << ")";
            }
            else if (e.kind == Entry::Row && e.meta->kind == uimeta::Slider)
                o << " (" << uikit::Narrow(SliderCaption(e, SliderValue(e))) << ")";
            if (Changed(e))
                o << " *";
        }
        else if (e.kind == Entry::Row && e.meta->preset)
        {
            const int pi = PickerIndex(e);
            o << " = (" << (pi >= 0 ? e.meta->options[pi] : "?") << ")";
        }
        if (!e.interactive)
            o << " [unavailable]";
        else if (ParentOff(e))
            o << " [parent off]";
        o << "\n";
    }
    const int n = static_cast<int>(g_entries.size());
    if (g_focus >= n)
        o << "> button " << (g_focus - n == 0 ? "DEFAULT" : g_focus - n == 1 ? "CANCEL" : "CLOSE") << "\n";
    return o.str();
}

std::string Command(std::istream& in)
{
    std::string sub, arg;
    in >> sub >> arg;
    if (sub == "options")
    {
        if (arg == "close")
            CloseAs(true);
        else if (arg == "cancel")
        {
            if (g_open)
                RevertAll();
            CloseAs(false);
        }
        else if (arg == "open" || !g_open)
        {
            if (!g_open)
                OpenAs(Host::Standalone);
        }
        else
            CloseAs(true);
        return g_open ? "open" : "closed";
    }
    if (sub == "tab")
    {
        if (!g_open)
            return "closed";
        int t = -1;
        for (int i = 0; i < uimeta::kTabCount; ++i)
            if (_stricmp(uimeta::kTabs[i].id, arg.c_str()) == 0 || _stricmp(uimeta::kTabs[i].label, arg.c_str()) == 0)
                t = i;
        if (t < 0 && !arg.empty() && isdigit(static_cast<unsigned char>(arg[0])))
            t = atoi(arg.c_str());
        if (t < 0 || t >= uimeta::kTabCount)
            return "tabs: 0 general, 1 controller, 2 autopilot, 3 subtitles, 4 movies";
        SwitchTab(t);
        return std::string("tab ") + uimeta::kTabs[g_tab].label;
    }
    if (sub == "rows")
        return g_open ? RowsReport() : "closed";
    if (sub == "press") // ui press up|down|left|right|a|b|lb|rb|pgup|pgdn [...]: the next frame's input
    {
        if (!g_open)
            return "closed";
        do
        {
            if (!arg.empty())
                g_pressQueue.push_back(arg);
        } while (in >> arg);
        return "queued " + std::to_string(g_pressQueue.size());
    }
    if (sub == "focus") // ui focus <key>: the focus onto a row of the current tab
    {
        for (size_t i = 0; i < g_entries.size(); ++i)
            if (g_entries[i].kind != Entry::Header &&
                ((g_entries[i].kind == Entry::Row && arg == g_entries[i].meta->key) || arg == g_entries[i].key))
            {
                g_focus = static_cast<int>(i);
                KeepFocusVisible();
                return "focus " + std::to_string(i);
            }
        return "no row " + arg;
    }
    if (sub == "default")
    {
        if (g_open)
            ResetTab();
        return g_open ? "defaults applied" : "closed";
    }
    if (sub == "state")
    {
        std::ostringstream o;
        o << "open=" << (g_open ? 1 : 0) << " host=" << (g_hostMode == Host::Options ? "options" : g_hostMode == Host::Standalone ? "standalone" : "none")
          << " tab=" << g_tab << " focus=" << g_focus << " capturing=" << (g_capturing ? 1 : 0)
          << " bus=" << (Bus() ? Bus()->Count() : -1) << " pageState="
          << (HostAtLeast(18) && g_host->OptionsPageState ? g_host->OptionsPageState() : -1)
          << " pad=" << (g_input.PadConnected() ? 1 : 0) << " screen=" << g_layout.sw << "x" << g_layout.sh
          << " tabs=" << uimeta::kTabCount << " settled=" << (g_settled ? 1 : 0);
        return o.str();
    }
    return "ui: options [open|close|cancel] | tab <n|name> | rows | press <keys...> | focus <key> | default | state";
}
} // namespace options_page
