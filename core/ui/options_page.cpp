// The FLVIBED OPTIONS page, simple mode (docs\UI_OPTIONS.md) - see options_page.h.
//
// Rows come from core\ui_meta.h (the spec's tabs by owner: kind, words, range, tier, needs, parent, help) and their values from the settings bus (core\settings_bus.h): no
// copy of any value lives here. A change goes to the bus at once (bus->Set: settings::Apply + the owner's `applied`); DEFAULT resets the tab's rows; CANCEL puts every value
// back to the snapshot taken when the page opened; leaving saves every section with a change (bus->Save, the owner's ini writer). A preset row (autopilot `preset`, controller
// `pad_preset`, the bundles) applies its table and reads CUSTOM when a member deviates. The rules (head states, what a head writes, what lists under YOU CHANGED) are
// core\ui\options_rules.h. The tier `simple` always shows; `show_advanced` (Show all settings) adds the other rows and every bus row the spec does not name, raw, on the tab of
// their owner; without it a hidden row away from its default comes back under YOU CHANGED. A key with no module on the bus is written to its file when the page keeps.
//
// Hosting: (a) inside the game's Options screen when the host switched to FLvibed's page (HostApi v18 OptionsPageState() == 2: the host hid the game's page controls; this draws
// the page in the panel area with the game's measured geometry, FREELANCER_RE "Options screen"); (b) standalone as an overlay panel from the pause menu's sixth button, the
// debug command `ui options` or Open(). The first-start card is the same machinery over the main menu, on four rows. Geometry = fractions of the screen height, times the
// page's text scale (HUD size and text_size), so every resolution, aspect and DPI works and the page previews HUD size.
#include "ui/options_page.h"

#include "game/fl.h"
#include "game/gamefont.h"
#include "game/menu.h"
#include "game/render.h"
#include "game/uidraw.h"
#include "log.h"
#include "game/hostcalls.h"
#include "game/hudsize.h"
#include "game/modslots.h"
#include "game/navview.h" // the map's wheel
#include "game/session.h" // MainMenuVisible: when the first-start card opens
#include "modules_rules.h"
#include "settings_reset.h" // the first card's stamp
#include "ui/views.h"
#include "ui_buttons.h"
#include "version.h"
#include "settings_bus.h"
#include "ui/ui.h" // ControlRect: the Options screen's panel ("big box") while it slides
#include "ui/hud_rows.h"
#include "ui/options_rules.h"
#include "ui/presets.h"  // the layout card's tables
#include "ui/winplates.h"
#include "ui/hud2_kit.h" // ui_style HUD 2: the tabs, buttons and focus in HUD 2's kit (modules\hud\docs\UI_HUD2_EXTENDED.md 10)
#include "ui/uiopt.h"
#include "ui_meta.h"
#include "ui/uikit.h"
#include "game/room.h" // uiclick::FindVisible
#include "ui/flufaware.h"    // the FLUF row's note

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>

namespace options_page
{
using optrules::Head;
using optrules::SameValue;
using uikit::Widen;

// ------------------------------------------------------------------ state
enum class Host
{
    None,
    Options,   // inside the game's Options screen (the host's page switch)
    Standalone // the overlay panel
};

// One line of the current tab: a section header, a spec row, or a raw registry row.
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
    bool indent = false;       // stands indented under its head (MORE)
    const char* head = nullptr; // the spec key of the head it stands under; null = none
    std::wstring offNote; // its module is off or stopped: `ui rows` says so,
    std::string offHelp;  // and the description says what to do
};

// What stands over the page, with the keys and the pointer to itself.
enum class Modal
{
    None,
    DisableController,
    ResetEverything,
    Layout
};

// A chip: DEFAULT, CANCEL, CLOSE and their kin. Each carries its button and is what a mouse clicks, so the page needs no second lane of prompts.
enum class ChipAct
{
    Default,
    Cancel,
    Close,
    LayoutPrev,
    LayoutNext,
    Confirm,
    Refuse
};
struct Chip
{
    float x = 0, y = 0, w = 0, h = 0;
    std::wstring word, key;
    glyphs::Id pad = glyphs::COUNT; // COUNT: a dialog's answer, no button of its own
    ChipAct act = ChipAct::Close;
    const char* help = "";
};

struct Layout
{
    float sw = 0, sh = 0, cx = 0;
    float k = 1;                     // the page's text scale
    float left = 0, right = 0;       // the plate the page stands on
    float panelTop = 0, panelBottom = 0;
    float titleY = 0, titleX = 0;    // the title's centre (0: no title, the hosted page: the column's lit FLVIBED button names it)
    float tabsY = 0, tabH = 0, tabsLeft = 0, tabsRight = 0;
    float tabX[uimeta::kTabCount] = {}, tabW[uimeta::kTabCount] = {};
    float rowsTop = 0, rowsBottom = 0, rowH = 0, pitch = 0;
    float labelX = 0, controlLeft = 0, controlRight = 0;
    float previewTop = 0, previewH = 0; // the subtitles' sample (0 = none)
    float previewGap = 0;               // the room it takes between the rows
    int previewAfter = 0;               // the rows above it
    float descTop = 0, descH = 0;       // the description band
    int visible = 0; // rows that fit
};

struct Snapshot
{
    std::string section, key, value;
};

static Host g_hostMode = Host::None;
static float g_previewNeedH = 0; // the sample's height, as DrawPreview measured it, for the next layout
static bool g_open = false;
static int g_tab = 0;
static int g_tabFocus[uimeta::kTabCount] = {}; // on return to a tab the focus is where it was
static int g_focus = 0;  // index into g_entries
static int g_scroll = 0; // the first visible entry
static int g_hover = -1; // the entry under the pointer (-1 none), -2 - i chip i, -10 - i tab i
static bool g_card = false;  // the first-start card, not the page
static Modal g_modal = Modal::None;
static int g_modalFocus = 1; // the dialog's answer: the safe one first
static int g_layoutShown = 0; // the layout card's layout
static std::vector<Chip> g_chips;  // what answers the pointer now
static std::vector<Chip> g_behind; // the page's own, behind what stands over it
static bool g_cardChecked = false; // this start looked for the first card's stamp
static std::vector<Entry> g_entries;
static std::vector<Snapshot> g_snapshot;
static bool g_capturing = false;   // a key row waits for a key
static uikit::Input g_input;
static Layout g_layout;
static bool g_dragging = false;    // the pointer drags a slider
// The scroll bar as Draw drew it (screen px), for the pointer: its arrows, track and thumb (the
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
    const std::string dir = std::string(DllDir().c_str()) + "..\\DATA\\MOVIES\\";
    for (int i = 1; i < 4; ++i)
        g_introHave[i] = GetFileAttributesA((dir + kIntroFiles[i]).c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool IsIntroRow(const uimeta::Row* m) { return m && m->key && strcmp(m->key, "movie_intro") == 0; }
static bool IntroAvailable(int option) { return option >= 0 && option < 4 ? g_introHave[option] : false; }

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
    if (hostcalls::g.GetSettingsBus)
        if (const SettingsBus* b = hostcalls::g.GetSettingsBus())
            return b;
    return localbus::Count() ? &localbus::g_bus : nullptr;
}

// A row with no owner on the bus, its module off: the value its file holds, read once per list of rows, so the page never shows a false OFF.
static std::map<std::string, std::string> g_fileRows;

static std::string SpecDefault(const std::string& section, const std::string& key)
{
    const uimeta::Row* row = uimeta::FindRegistryRow(section.c_str(), key.c_str());
    return row && row->def ? row->def : "";
}

// A key no module owns right now, as the page keeps it: written to its file when the page closes and kept, forgotten by CANCEL. `gone`: the key leaves the file (a reset).
struct FileWrite
{
    std::string section, key, value;
    bool gone = false;
};
static std::map<std::string, FileWrite> g_fileWrites;

static std::string IniOf(const std::string& section) { return std::string(DllDir().c_str()) + (section == "Subtitles" ? "FLSubtitles.ini" : "FLController.ini"); }

static std::string FileRow(const std::string& section, const std::string& key)
{
    const std::string id = section + "." + key;
    const auto pending = g_fileWrites.find(id);
    if (pending != g_fileWrites.end())
        return pending->second.gone ? SpecDefault(section, key) : pending->second.value;
    const auto have = g_fileRows.find(id);
    if (have != g_fileRows.end())
        return have->second;
    char buf[128] = {};
    const bool found = section != "Modules" && GetPrivateProfileStringA(section.c_str(), key.c_str(), "", buf, sizeof(buf), IniOf(section).c_str()) > 0;
    return g_fileRows[id] = found ? buf : SpecDefault(section, key);
}

static void FlushFileWrites()
{
    for (const auto& w : g_fileWrites)
        WritePrivateProfileStringA(w.second.section.c_str(), w.second.key.c_str(), w.second.gone ? nullptr : w.second.value.c_str(), IniOf(w.second.section).c_str());
    g_fileWrites.clear();
    g_fileRows.clear();
}

static std::string Get(const std::string& section, const std::string& key)
{
    char buf[128] = {};
    const SettingsBus* bus = Bus();
    if (bus && bus->Get(section.c_str(), key.c_str(), buf, sizeof(buf)))
        return buf;
    return FileRow(section, key);
}

static std::string Default(const std::string& section, const std::string& key)
{
    char buf[128] = {};
    const SettingsBus* bus = Bus();
    if (bus && bus->Default(section.c_str(), key.c_str(), buf, sizeof(buf)))
        return buf;
    return SpecDefault(section, key);
}

static void Toast(const std::string& s);

static bool Want(int row) { return atof(Get("Modules", modules::kRowNames[row]).c_str()) != 0; }

static bool BusSet(const std::string& section, const std::string& key, const std::string& value)
{
    const SettingsBus* bus = Bus();
    const bool ok = bus && bus->Set(section.c_str(), key.c_str(), value.c_str());
    if (!ok)
        Log("options page: set %s.%s = %s refused", section.c_str(), key.c_str(), value.c_str());
    return ok;
}

// A value on the bus when a module owns its key, else for the file when the page keeps: a head's live key must reach the next start whichever modules run now.
static void SetAnywhere(const std::string& section, const std::string& key, const std::string& value)
{
    const SettingsBus* bus = Bus();
    if (bus && bus->Find(section.c_str(), key.c_str()))
        BusSet(section, key, value);
    else
        g_fileWrites[section + "." + key] = {section, key, value, false};
}

// A row of [Modules] the rule "one of Subtitles and Controller stays on" (options_rules.h PlanHead) has a word on. With both off nothing of FLvibed starts, this page included.
static void SetModule(const char* name, bool on)
{
    const bool subtitles = !strcmp(name, "subtitles"), controller = !strcmp(name, "controller");
    const optrules::Feature feature = subtitles ? optrules::Feature::Subtitles : controller ? optrules::Feature::Controller : optrules::Feature::Other;
    const bool other = feature != optrules::Feature::Other && Want(subtitles ? modules::kController : modules::kSubtitles);
    const optrules::HeadPlan p = optrules::PlanHead(feature, false, on, other, true);
    if (p.setModule)
        BusSet("Modules", name, p.module ? "1" : "0");
    else
        SetAnywhere("Subtitles", "enabled", "0"); // the Subtitles module stays loaded, showing nothing
    if (p.subtitlesStayLoaded)
    {
        BusSet("Modules", "subtitles", "1");
        SetAnywhere("Subtitles", "enabled", "0");
    }
}

static bool Set(const std::string& section, const std::string& key, const std::string& value)
{
    if (section == "Modules")
    {
        SetModule(key.c_str(), atof(value.c_str()) != 0);
        return true;
    }
    return BusSet(section, key, value);
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
static bool ShowAdvanced() { return atof(Get("Controller", "show_advanced").c_str()) != 0; }

static bool TabIs(const char* id) { return g_tab >= 0 && g_tab < uimeta::kTabCount && !strcmp(uimeta::kTabs[g_tab].id, id); }

// The [Modules] row that a row belongs to; -1 = core's: it acts whatever is on. A row of `[Controller]` that a cut module reads has its home with that module
// (core\ui\shared_rows.h, core\ui\hud_rows.h); the rest of a section is that section's module.
static int RowOwner(const char* section, const char* key)
{
    static const struct
    {
        const char* key;
        int row;
    } kRows[] = {{"borderless", modules::kDisplay},        {"run_in_background", modules::kDisplay},  {"speedrun_timer", modules::kSpeedrun},
                 {"livesplit", modules::kSpeedrun},        {"livesplit_port", modules::kSpeedrun},    {"livesplit_gametime", modules::kSpeedrun},
                 {"movie_fit", modules::kMovies},          {"movie_skip", modules::kMovies},          {"movie_intro", modules::kMovies},
                 {"npc_stall_log", modules::kNpc},         {"maneuver_button", modules::kAutopilot},  {"autopilot_key", modules::kAutopilot},
                 {"autopilot_cancel", modules::kAutopilot}, {"ap_hints", modules::kAutopilot},        {"maneuver_watchdog", -1},
                 {"text_size", -1},                        {"hud_size", -1},                          {"fluf_windows", -1},
                 {"show_advanced", -1},                    {"button_hints", modules::kHud},           {"hit_effects", modules::kHud}};
    for (const auto& r : kRows)
        if (!strcmp(key, r.key))
            return r.row;
    for (const settings::Def& d : hudrows::kRows)
        if (!strcmp(key, d.name))
            return modules::kHud;
    if (!strcmp(section, "Modules"))
        return -1;
    return !strcmp(section, "Autopilot") ? modules::kAutopilot : !strcmp(section, "Subtitles") ? modules::kSubtitles : modules::kController;
}

// The flight's own rows: the pad's, and the autopilot's math reads them too.
static bool AutopilotReads(const char* key) { return !strcmp(key, "steer_scale") || !strcmp(key, "aim_cone") || !strcmp(key, "invert_y"); }

// The module's words in `ui rows` and the description, when it is off or a fault stopped it.
static void NoteModule(Entry& e)
{
    static const struct
    {
        const wchar_t* note;
        const char* name;
        int slot;
    } kWords[modules::kRows] = {{L"SUBTITLES MODULE OFF", "Subtitles", -1},  {L"CONTROLLER MODULE OFF", "Controller", -1},
                                {L"HUD MODULE OFF", "HUD", modslots::kHud},  {L"AUTOPILOT MODULE OFF", "Autopilot", modslots::kAutopilot},
                                {L"SPEEDRUN MODULE OFF", "Speedrun", modslots::kSpeedrun}, {L"DISPLAY MODULE OFF", "Display", -1},
                                {L"NPC MODULE OFF", "NPC", modslots::kNpc},  {L"MOVIES MODULE OFF", "Movies", -1}};
    if (!hostcalls::g.ModuleOn || (e.kind == Entry::Row && e.meta->head))
        return;
    const char* key = !e.key.empty() ? e.key.c_str() : e.kind == Entry::Row ? e.meta->key : "";
    if (e.key.empty() && !(e.kind == Entry::Row && e.meta->preset)) // (a button of the page's own)
        return;
    const int row = RowOwner(e.section.c_str(), key);
    if (row < 0)
        return;
    const auto& w = kWords[row];
    const std::string name = w.name;
    if (hostcalls::g.ModuleOn(row))
    {
        if (w.slot >= 0 && modslots::g_fault[w.slot])
        {
            e.offNote = L"MODULE STOPPED";
            e.offHelp = "It stopped after a fault. Restart the game.";
        }
        return;
    }
    if (AutopilotReads(key) && hostcalls::g.ModuleOn(modules::kAutopilot))
        return;
    e.offNote = w.note;
    e.offHelp = "The " + name + " module is off, so this row does nothing: it shows the value in the file.";
}

static Entry RowEntry(const uimeta::Row& row, const char* head, bool indent)
{
    const SettingsBus* bus = Bus();
    Entry e;
    e.kind = Entry::Row;
    e.meta = &row;
    e.label = Widen(row.label);
    e.help = row.help;
    e.section = row.section;
    e.key = row.regKey ? row.regKey : "";
    e.def = (bus && row.regKey) ? bus->Find(row.section, row.regKey) : nullptr;
    e.head = head;
    e.indent = indent && !row.head;
    // a row with a registry key the bus does not have (its module is off): shown dimmed, so the spec's rows are all visible and the gap is honest
    e.interactive = row.kind == uimeta::Button || !row.regKey || e.def != nullptr;
    NoteModule(e);
    return e;
}

static void HeaderEntry(const std::wstring& label)
{
    Entry h;
    h.kind = Entry::Header;
    h.label = label;
    g_entries.push_back(h);
}

static const uimeta::PresetSet* PresetsOf(const uimeta::Row& row) { return row.preset ? uimeta::FindPresets(row.key) : nullptr; }

static int PresetIndexOf(const uimeta::Row& row);

// A hidden row a preset covers: while the preset reads a word the row's value is that word's, and the row needs no line of its own.
static bool NamedByPreset(const uimeta::Row& row)
{
    if (!row.regKey)
        return false;
    for (const uimeta::PresetSet& s : uimeta::kPresetSets)
    {
        const uimeta::Row* owner = uimeta::FindRow(s.rowKey);
        if (!owner || PresetIndexOf(*owner) == owner->optionCount - 1)
            continue;
        for (int m = 0; m < s.presets[0].count; ++m)
            if (!strcmp(s.presets[0].values[m].key, row.regKey))
                return true;
    }
    return false;
}

// The tab whose head a bus row with no spec row of its own stands under, by its owner: a module with no tab lives on MORE.
static int TabOfOwner(int row)
{
    const char* id = row == modules::kHud ? "hud" : row == modules::kController ? "controller" : row == modules::kSubtitles ? "subtitles" : row == modules::kAutopilot ? "autopilot" : "more";
    for (int t = 0; t < uimeta::kTabCount; ++t)
        if (!strcmp(uimeta::kTabs[t].id, id))
            return t;
    return 0;
}

// The spec key of the head of a [Modules] row (-1: none).
static const char* HeadOfOwner(int row)
{
    if (row < 0)
        return nullptr;
    for (const uimeta::Tab& t : uimeta::kTabs)
        for (int s = 0; s < t.count; ++s)
            for (int r = 0; r < t.sections[s].count; ++r)
            {
                const uimeta::Row& x = t.sections[s].rows[r];
                if (x.head && !strcmp(x.head, modules::kRowNames[row]))
                    return x.key;
            }
    return nullptr;
}

// A registry row the spec does not name and no row lists: raw under Show all settings, a count on MORE otherwise.
template <class Fn>
static void ForEachRawRow(Fn fn)
{
    const SettingsBus* bus = Bus();
    for (int i = 0; bus && i < bus->Count(); ++i)
    {
        const SettingsBusSection* sec = bus->Section(i);
        for (size_t r = 0; sec && r < sec->count; ++r)
            if (!uimeta::FindRegistryRow(sec->section, sec->defs[r].name))
                fn(*sec, sec->defs[r]);
    }
}

static int OtherChanged()
{
    int n = 0;
    ForEachRawRow([&n](const SettingsBusSection& sec, const settings::Def& d)
                  { n += !SameValue(Get(sec.section, d.name), Default(sec.section, d.name)); });
    return n;
}

static void BuildCardEntries()
{
    for (int i = 0; i < uimeta::kCardRowCount; ++i)
    {
        const uimeta::CardRow& c = uimeta::kCardRows[i];
        const uimeta::Row* row = !strcmp(c.key, uimeta::kCardSubtitles.key) ? &uimeta::kCardSubtitles : uimeta::FindRow(c.key);
        if (!row)
            continue;
        Entry e = RowEntry(*row, nullptr, false);
        e.help = c.help;
        g_entries.push_back(e);
    }
}

static void BuildEntries()
{
    g_entries.clear();
    g_fileRows.clear();
    if (g_tab < 0 || g_tab >= uimeta::kTabCount)
        g_tab = 0;
    const bool advanced = ShowAdvanced();
    g_lastAdvanced = advanced;
    if (g_card)
    {
        BuildCardEntries();
        return;
    }
    const uimeta::Tab& tab = uimeta::kTabs[g_tab];
    const int otherChanged = OtherChanged();
    std::vector<Entry> changed; // YOU CHANGED
    for (int s = 0; s < tab.count; ++s)
    {
        const uimeta::Section& sec = tab.sections[s];
        bool header = !sec.label || !*sec.label; // (no header wanted)
        for (int r = 0; r < sec.count; ++r)
        {
            const uimeta::Row& row = sec.rows[r];
            // (the one row by pad: CONTROLLER is full at Large, and the handheld that sets Large has no such triggers; docs\UI_OPTIONS.md 1)
            const bool shown = row.tier == uimeta::Simple || advanced || (uiopt::Get().padTriggers && !strcmp(row.key, "trigger_effects"));
            if (!strcmp(row.key, "other_changed") && otherChanged == 0)
                continue;
            if (!shown)
            {
                const bool alias = row.regKey && uimeta::FindRegistryRow(row.section, row.regKey) != &row; // the same key stands on another row
                const bool differs = row.regKey && !alias && !SameValue(Get(row.section, row.regKey), Default(row.section, row.regKey));
                if (differs && optrules::ListChanged(true, differs, NamedByPreset(row)))
                    changed.push_back(RowEntry(row, sec.head, false));
                continue;
            }
            if (!header)
            {
                HeaderEntry(Widen(sec.label));
                header = true;
            }
            g_entries.push_back(RowEntry(row, sec.head, sec.indent));
            if (!strcmp(row.key, "other_changed"))
                g_entries.back().label = Widen(("Other changed settings: " + std::to_string(otherChanged)).c_str());
        }
    }
    if (!changed.empty())
    {
        HeaderEntry(L"YOU CHANGED");
        g_entries.insert(g_entries.end(), changed.begin(), changed.end());
    }
    // Show all settings: every bus row the spec does not name, raw, on the tab of its owner
    if (!advanced)
        return;
    bool header = false;
    ForEachRawRow([&](const SettingsBusSection& sec, const settings::Def& d)
                  {
                      const int owner = RowOwner(sec.section, d.name);
                      if (TabOfOwner(owner) != g_tab)
                          return;
                      if (!header)
                      {
                          HeaderEntry(L"ADVANCED");
                          header = true;
                      }
                      Entry e;
                      e.kind = Entry::RawRow;
                      e.def = &d;
                      e.section = sec.section;
                      e.key = d.name;
                      e.label = Widen(d.name);
                      e.help = d.help ? d.help : "";
                      e.head = HeadOfOwner(owner);
                      e.interactive = true;
                      g_entries.push_back(e);
                  });
}

// Everything but a section label takes the focus: a row that cannot change says why in the description.
static bool Interactive(int i)
{
    return i >= 0 && i < static_cast<int>(g_entries.size()) && g_entries[i].kind != Entry::Header;
}

// The parent row (the spec's `parent` key) is off: the row is greyed and inert.
static bool ParentOff(const Entry& e)
{
    if (e.kind != Entry::Row || !e.meta->parent || e.meta->head)
        return false;
    const uimeta::Row* p = uimeta::FindRow(e.meta->parent);
    if (!p || !p->regKey || p->head) // (a head has its own word on its rows)
        return false;
    return atof(Get(p->section, p->regKey).c_str()) == 0;
}

// ------------------------------------------------------------------ the heads
static int ModuleRow(const uimeta::Row& head)
{
    for (int r = 0; head.head && r < modules::kRows; ++r)
        if (!strcmp(modules::kRowNames[r], head.head))
            return r;
    return -1;
}

static bool TwoKeys(const uimeta::Row& head) { return head.regKey && strcmp(head.section, "Modules") != 0; }

static bool Faulted(int row)
{
    const int slot = row == modules::kHud ? modslots::kHud : row == modules::kAutopilot ? modslots::kAutopilot : row == modules::kSpeedrun ? modslots::kSpeedrun
                     : row == modules::kNpc ? modslots::kNpc : -1;
    return slot >= 0 && modslots::g_fault[slot] != 0;
}

static Head HeadStateOf(const uimeta::Row& head)
{
    const int row = ModuleRow(head);
    optrules::HeadKeys k;
    if (row < 0)
        return Head::Enabled;
    k.twoKeys = TwoKeys(head);
    k.live = k.twoKeys && atof(Get(head.section, head.regKey).c_str()) != 0;
    k.want = Want(row);
    k.running = hostcalls::g.ModuleOn ? hostcalls::g.ModuleOn(row) : true;
    k.fault = Faulted(row);
    return optrules::HeadState(k);
}

static const uimeta::Row* HeadRowOf(const Entry& e)
{
    if (!e.head || (e.kind == Entry::Row && e.meta->head))
        return nullptr;
    return uimeta::FindRow(e.head);
}

// The row cannot change now: under a head that is not ENABLED, or its owner is not on the bus.
static bool Dimmed(const Entry& e)
{
    if (e.kind == Entry::Header || (e.kind == Entry::Row && e.meta->head))
        return false;
    if (const uimeta::Row* head = HeadRowOf(e))
        if (!optrules::RowsLive(HeadStateOf(*head)))
            return true;
    return !e.interactive || !e.offNote.empty();
}

// What the description says of a dimmed row.
static std::string DimHelp(const Entry& e)
{
    if (const uimeta::Row* head = HeadRowOf(e))
        switch (HeadStateOf(*head))
        {
        case Head::Disabled: return head->offHelp ? head->offHelp : "";
        case Head::DisabledAtNextStart: return "It keeps working until you restart.";
        case Head::EnabledAtNextStart: return "It comes on at the next start. Your setting is kept.";
        case Head::Stopped: return "It stopped after a fault. Restart the game.";
        default: break;
        }
    return e.offHelp;
}

// Acting on a head: `pick` for the row with three words, the option chosen; else the wanted state. `confirmed`: the Controller's dialog was answered.
static void ActHead(const uimeta::Row& head, bool on, int pick, bool confirmed)
{
    const int row = ModuleRow(head);
    if (row < 0)
        return;
    const bool subtitles = row == modules::kSubtitles, controller = row == modules::kController;
    const optrules::Feature feature = subtitles ? optrules::Feature::Subtitles : controller ? optrules::Feature::Controller : optrules::Feature::Other;
    const bool other = feature != optrules::Feature::Other && Want(subtitles ? modules::kController : modules::kSubtitles);
    const optrules::HeadPlan p = optrules::PlanHead(feature, TwoKeys(head), on, other, confirmed);
    if (p.ask)
    {
        g_modal = Modal::DisableController;
        g_modalFocus = 1;
        return;
    }
    if (p.setLive)
    {
        const int option = pick >= 0 ? pick : p.live ? 0 : head.optionCount - 1; // (a head with words: its first is on, its last off)
        SetAnywhere(head.section, head.regKey, head.values ? head.values[option] : p.live ? "1" : "0");
    }
    SetModule(modules::kRowNames[row], on);
    Log("options page: head %s -> %s", head.key, on ? "on" : "off");
}

// ------------------------------------------------------------------ presets
// A member a module owns now: the others cannot be set, so they do not decide the word.
static bool MemberOwned(const uimeta::PresetValue& m)
{
    const SettingsBus* bus = Bus();
    return bus && bus->Find(m.section, m.key);
}

// The preset row's option index: the matching preset, else the last option (CUSTOM). A real row (pad_preset) names its preset.
static int PresetIndexOf(const uimeta::Row& row)
{
    const int custom = row.optionCount - 1;
    const uimeta::PresetSet* s = PresetsOf(row);
    if (!s)
        return custom;
    const int chosen = row.regKey ? atoi(Get(row.section, row.regKey).c_str()) : -1;
    return optrules::PresetWord(s->presets, s->count, custom, chosen, [](const uimeta::PresetValue& m) { return Get(m.section, m.key); }, MemberOwned);
}

static int PresetIndex(const Entry& e) { return PresetIndexOf(*e.meta); }

static void SetPreset(const Entry& e, int index)
{
    const uimeta::PresetSet* s = PresetsOf(*e.meta);
    if (!s || index < 0 || index >= s->count)
        return;
    if (e.meta->regKey)
        Set(e.section, e.key, std::to_string(index));
    const uimeta::Preset& p = s->presets[index];
    for (int i = 0; i < p.count; ++i)
        if (MemberOwned(p.values[i]))
            Set(p.values[i].section, p.values[i].key, p.values[i].value);
    Log("options page: preset %s", p.name);
}

// ------------------------------------------------------------------ row values
// The card's Subtitles row: off is the head, a word is the head on and `size`.
static bool IsCardSubtitles(const Entry& e) { return e.kind == Entry::Row && e.meta == &uimeta::kCardSubtitles; }

static const uimeta::Row* SubtitlesHead()
{
    static const uimeta::Row* const head = uimeta::FindRow("enabled");
    return head;
}

// The picker's option index of a spec row (-1 = the registry holds a value no option names).
static int PickerIndex(const Entry& e)
{
    if (e.meta->preset)
        return PresetIndex(e);
    if (IsCardSubtitles(e))
    {
        const Head h = SubtitlesHead() ? HeadStateOf(*SubtitlesHead()) : Head::Disabled;
        return h == Head::Enabled || h == Head::EnabledAtNextStart ? 1 + atoi(Get("Subtitles", "size").c_str()) : 0;
    }
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
        // CUSTOM has no table to apply: the row reads it when a member differs, and either step from it lands on a word (docs\UI_OPTIONS.md 6.2)
        const uimeta::PresetSet* s = PresetsOf(*e.meta);
        const int n = s ? s->count : 0;
        if (n > 0)
            SetPreset(e, i >= n ? 0 : i < 0 ? n - 1 : i);
        return;
    }
    const int n = e.meta->optionCount;
    i = ((i % n) + n) % n;
    if (IsCardSubtitles(e))
    {
        if (SubtitlesHead())
            ActHead(*SubtitlesHead(), i != 0, -1, true);
        if (i != 0)
            SetAnywhere("Subtitles", "size", std::to_string(i - 1));
        return;
    }
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
    if (e.kind == Entry::Row && e.meta->preset && !e.meta->regKey)
        return PresetIndex(e) != 0; // a bundle's first word is its default
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
    FlushFileWrites();
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
    const SettingsBus* bus = Bus(); // (not Set: going back to where the page opened is no player's choice to refuse)
    g_fileWrites.clear();
    int n = 0;
    for (const Snapshot& s : g_snapshot)
        if (bus && !SameValue(Get(s.section, s.key), s.value))
            n += bus->Set(s.section.c_str(), s.key.c_str(), s.value.c_str()) ? 1 : 0;
    Log("options page: cancel reverted %d value(s)", n);
}

// A row back to its default: on the bus, or for its file when no module owns it now.
static bool ResetRow(const char* section, const char* key)
{
    const SettingsBus* bus = Bus();
    if (bus && bus->Find(section, key))
        return bus->Reset(section, key);
    if (!strcmp(section, "Modules"))
        return false;
    g_fileWrites[std::string(section) + "." + key] = {section, key, "", true};
    return true;
}

// DEFAULT: this tab's rows, heads and bundles and YOU CHANGED rows included, back at once. A bundle's members are hidden rows, so its row resets them.
static void ResetTab()
{
    int n = 0;
    for (const Entry& e : g_entries)
    {
        if (e.kind == Entry::Header)
            continue;
        if (e.kind == Entry::Row && e.meta->head)
            n += ResetRow("Modules", e.meta->head) ? 1 : 0;
        if (IsCardSubtitles(e))
        {
            ResetRow("Modules", "subtitles");
            ResetRow("Subtitles", "enabled");
            ResetRow("Subtitles", "size");
            continue;
        }
        if (!e.key.empty())
            n += ResetRow(e.section.c_str(), e.key.c_str()) ? 1 : 0;
        if (e.kind == Entry::Row && e.meta->preset)
            if (const uimeta::PresetSet* s = PresetsOf(*e.meta))
                for (int m = 0; m < s->presets[0].count; ++m)
                    n += ResetRow(s->presets[0].values[m].section, s->presets[0].values[m].key) ? 1 : 0;
    }
    Log("options page: default reset %d row(s) of tab %s", n, g_card ? "card" : uimeta::kTabs[g_tab].label);
}

// RESET THEM: the keys no row lists. CANCEL still takes it back.
static void ResetOthers()
{
    const SettingsBus* bus = Bus();
    int n = 0;
    ForEachRawRow([&](const SettingsBusSection& sec, const settings::Def& d)
                  {
                      if (!SameValue(Get(sec.section, d.name), Default(sec.section, d.name)))
                          n += bus->Reset(sec.section, d.name) ? 1 : 0;
                  });
    Log("options page: reset %d other row(s)", n);
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
    // A module that is off has no registry on the bus: its rows go from the file, or they would come back with the module, which this reset has just turned on. They go when
    // the page keeps, so CANCEL reverses this too.
    int gone = 0;
    for (int t = 0; t < uimeta::kTabCount; ++t)
        for (int s = 0; s < uimeta::kTabs[t].count; ++s)
            for (int r = 0; r < uimeta::kTabs[t].sections[s].count; ++r)
            {
                const uimeta::Row& row = uimeta::kTabs[t].sections[s].rows[r];
                if (!row.regKey || !strcmp(row.section, "Modules") || bus->Find(row.section, row.regKey))
                    continue;
                gone += ResetRow(row.section, row.regKey) ? 1 : 0;
            }
    Log("options page: reset everything (%d rows, %d from the files)", n, gone);
    Toast("Every FLvibed setting is back to its default");
}

static void SuppressPauseMenu(bool on)
{
    if (!hostcalls::g.SetHudHitSuppression)
        return;
    if (on)
    {
        // under the standalone panel the game's own buttons must not take the clicks: the pause menu's, the main menu's under the first-start card
        const uintptr_t root = g_card ? session::MainMenuRoot() : menu::PauseMenu();
        if (!root)
            return;
        hostcalls::g.SetHudHitSuppression(true, &root, 1);
        g_suppressed = true;
    }
    else if (g_suppressed)
    {
        hostcalls::g.SetHudHitSuppression(false, nullptr, 0);
        g_suppressed = false;
    }
}

// The focus stands on a row: never past the list's end or on a section label.
static void SettleFocus()
{
    const int n = static_cast<int>(g_entries.size());
    g_focus = std::clamp(g_focus, 0, std::max(0, n - 1));
    while (g_focus < n && !Interactive(g_focus))
        ++g_focus;
    if (g_focus >= n)
        g_focus = 0;
}

static void OpenAs(Host how)
{
    g_hostMode = how;
    g_open = true;
    g_modal = Modal::None;
    g_focus = g_card ? 0 : g_tabFocus[std::clamp(g_tab, 0, uimeta::kTabCount - 1)];
    g_scroll = 0;
    g_capturing = false;
    g_dragging = false;
    g_padDrove = false;
    g_lastPx = g_lastPy = -1;
    g_input.Reset();
    g_fileWrites.clear();
    TakeSnapshot();
    RefreshIntroFiles();
    BuildEntries();
    SettleFocus();
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
    if (!g_card)
        g_tabFocus[g_tab] = g_focus;
    g_open = false;
    g_card = false;
    g_modal = Modal::None;
    g_capturing = false;
    SuppressPauseMenu(false);
    if (g_hostMode == Host::Options && hostcalls::g.OptionsPageState &&
        hostcalls::g.OptionsPageState() == 2 && hostcalls::g.ShowOptionsPage)
        hostcalls::g.ShowOptionsPage(false); // the game's page back
    g_hostMode = Host::None;
    g_input.Reset();
    Log("options page: closed (%s)", keep ? "kept" : "reverted");
}

// Opens the page where it belongs: with the game's Options screen up, hosted in its panel (the host switches to FLvibed's
// page and Update opens it hosted) - never the standalone panel on top of the screen (hotfix 0.2.5: the spec capture's
// "off-centre" page was the standalone panel, centred on the screen, over the Options screen's right-of-centre panel);
// else the standalone panel. Returns false when the host switch was asked for (the page opens at the next Update).
static bool OpenBest()
{
    if (g_open)
        return true;
    const int state = hostcalls::g.OptionsPageState ? hostcalls::g.OptionsPageState() : 0;
    if (state == 1 && hostcalls::g.ShowOptionsPage)
    {
        hostcalls::g.ShowOptionsPage(true);
        Log("options page: the Options screen is open - hosted there");
        return false;
    }
    OpenAs(state == 2 ? Host::Options : Host::Standalone);
    return true;
}

void Open()
{
    OpenBest();
}
void Close() { CloseAs(true); }
bool IsOpen() { return g_open; }
bool WantsInput() { return g_open; }

void Init()
{
    g_input.Init();
    g_input.SetRepeat(optrules::kRepeatFirst, optrules::kRepeatNext);
    g_input.SetPageKeys(true);
}

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
    const bool button = hostcalls::g.UiButtonRect &&
                        hostcalls::g.UiButtonRect(kUiBtnOptionsPage, &r[0], &r[1], &r[2], &r[3]);
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

// The player's text_size as the page draws it: the same clamp as HUD 2's (80..150 %). hud_size's Large is in it already (hudsize::Apply).
static int TextPercent() { return std::clamp(uiopt::Get().textSize, 80, 150); }

// The page's own text follows HUD size, so the page previews that row; Small's base keeps the handheld's floor (hudsize::BaseK).
static float PageScale() { return TextPercent() / 100.0f * hudsize::g_baseK; }

// The chips along a plate's foot, equal widths; a chip's word follows its button, the keyboard's key beside it.
static void FootChips(const Layout& l, float bottom, std::initializer_list<Chip> items)
{
    const float H = l.sh, h = 0.048f * H * l.k, gap = 0.014f * H, x0 = l.left + 0.020f * H;
    const float w = (l.right - l.left - 0.040f * H - gap * (items.size() - 1)) / items.size();
    float x = x0;
    for (Chip c : items)
    {
        c.x = x, c.y = bottom - h - 0.012f * H, c.w = w, c.h = h;
        g_chips.push_back(c);
        x += w + gap;
    }
}

static Chip MakeChip(const wchar_t* word, glyphs::Id pad, const wchar_t* key, ChipAct act, const char* help)
{
    Chip c;
    c.word = word, c.pad = pad, c.key = key, c.act = act, c.help = help;
    return c;
}

// Tab widths by their words: room for the longest at Large with no measuring pass, since the layout runs before the fonts.
static void LayTabs(Layout& l)
{
    int chars = 0;
    for (const uimeta::Tab& t : uimeta::kTabs)
        chars += static_cast<int>(strlen(t.label)) + 2;
    const float gap = 0.012f * l.sh, room = l.tabsRight - l.tabsLeft - gap * (uimeta::kTabCount - 1);
    float x = l.tabsLeft;
    for (int t = 0; t < uimeta::kTabCount; ++t)
    {
        l.tabW[t] = room * (strlen(uimeta::kTabs[t].label) + 2) / chars;
        l.tabX[t] = x;
        x += l.tabW[t] + gap;
    }
}

// The screen's numbers (docs\UI_OPTIONS.md 8 and tools\uimock\options.py): the plate, a tab strip, rows, the description band, the chips. Rows, tabs and chips grow with the
// text, the widths stay the plate's: a word that no longer fits takes the fit rule, and rows that no longer fit scroll.
// The Options window's frame as it is drawn, pixels (x0 y0 x1 y1): the skin's measured plate on the game's own box (OptionsBox in game, big box from the
// main menu), the same rectangle RenderUnder fills. False while the box is not up.
static bool HostedPlate(float out[4])
{
    const char* anchor = uiclick::FindVisible("OptionsBox") ? "OptionsBox" : "big box"; // in game / the main menu
    const u32 box = uiclick::FindVisible(anchor);
    float off[4];
    ui::Item it;
    int w = 0, h = 0;
    if (!box || !winplates::PlateOffsets("options", anchor, off) || !ui::ControlRect(box, it) || !render::ScreenSize(w, h) || h < 100)
        return false;
    const float H = static_cast<float>(h);
    out[0] = it.x + off[0] * H, out[1] = it.y + off[1] * H, out[2] = it.x + off[2] * H, out[3] = it.y + off[3] * H;
    return true;
}

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
    const float H = l.sh, k = PageScale();
    l.k = k;
    float stripTop = 0;
    if (g_card)
    {
        // over the main menu, one plate, its title and lead above the sample
        const float halfW = std::min(0.55f * H, l.sw * 0.49f);
        l.left = l.cx - halfW, l.right = l.cx + halfW;
        l.panelTop = 0.12f * H, l.panelBottom = 0.83f * H;
        l.titleX = l.cx, l.titleY = l.panelTop + 0.036f * H * k;
        stripTop = l.panelTop + 0.104f * H * k;
    }
    else if (g_hostMode == Host::Options)
    {
        // the game's panel: 0.825 H wide, 0.125..0.965 H, its left edge 0.215 H left of the screen's centre at 16:10 and 16:9 (measured frames, tools\uimock\options.py).
        // No title: the lit FLVIBED button in the game's column names the page. In game the panel stands a little lower.
        const float ig = InGame() ? 0.012f : 0.0f;
        l.left = l.cx - 0.215f * H;
        l.right = std::min(l.cx + 0.610f * H, l.sw - 0.01f * H);
        l.panelTop = (0.125f + ig) * H;
        l.panelBottom = 0.965f * H;
        // ... but the page stands in the window's frame as it is drawn (Shockwave via the owner, 7 Nov 2026: "In-game FLvibed menu clips pretty roughly" -
        // the tabs on the frame's top edge, the chips under its foot, the rows on its left line: the measured panel is 0.825 H wide and 0.125..0.965 H,
        // the frame drawn round it - the skin's plate, the one RenderUnder fills - 0.77 H and 0.150..0.920 H in game). The plate read live, as RenderUnder does
        float plate[4];
        if (HostedPlate(plate))
            l.left = plate[0], l.panelTop = plate[1], l.right = plate[2], l.panelBottom = plate[3];
        stripTop = l.panelTop + 0.012f * H;
    }
    else
    {
        const float halfW = std::min(0.52f * H, l.sw * 0.49f);
        l.left = l.cx - halfW;
        l.right = l.cx + halfW;
        l.panelTop = 0.028f * H;
        l.panelBottom = 0.972f * H;
        l.titleX = l.cx;
        l.titleY = l.panelTop + 0.036f * H * k;
        stripTop = l.panelTop + 0.074f * H * k;
    }
    l.rowH = (0.046f + 0.035f * std::max(0.0f, k - 1)) * H;
    l.pitch = l.rowH + 0.004f * H;
    l.labelX = l.left + 0.034f * H;
    l.controlRight = l.right - 0.032f * H;
    l.controlLeft = l.controlRight - 0.32f * H;
    // the tab strip, LB and RB beside it
    l.tabH = 0.052f * H * k;
    l.tabsY = stripTop + l.tabH * 0.5f;
    const float gl = 0.040f * H * k;
    l.tabsLeft = l.left + 0.016f * H + gl;
    l.tabsRight = l.right - 0.016f * H - gl;
    // hosted, the window's own close X (PreferencesCloseButton) stands in the frame's top-right corner, on the strip's line: RB and the tabs end left of it,
    // and the first row starts under its foot (at small sizes it reached the head row's box)
    float closeFoot = 0;
    if (g_hostMode == Host::Options && !g_card)
    {
        ui::Item x;
        if (const u32 close = uiclick::FindVisible("PreferencesCloseButton"))
            if (ui::ControlRect(close, x) && x.hh > 0.5f && x.y + x.hh > l.tabsY - l.tabH * 0.5f && x.y - x.hh < l.tabsY + l.tabH * 0.5f)
                l.tabsRight = std::min(l.tabsRight, x.x - x.hw - 0.008f * H - gl), closeFoot = x.y + x.hh + 0.004f * H;
    }
    LayTabs(l);
    // the chips, bottom up
    g_chips.clear();
    if (g_card)
        FootChips(l, l.panelBottom, {MakeChip(L"DEFAULT", glyphs::X, L"D", ChipAct::Default, "These four rows back to their defaults."),
                                     MakeChip(L"CLOSE", glyphs::Menu(glyphs::B), L"ESC", ChipAct::Close, "Keeps what is set here, and goes on.")});
    else
        FootChips(l, l.panelBottom, {MakeChip(L"DEFAULT", glyphs::X, L"D", ChipAct::Default, "This tab back to its defaults, at once. CANCEL takes it back."),
                                     MakeChip(L"CANCEL", glyphs::Y, L"C", ChipAct::Cancel, "Every change since the page opened is undone, and it closes."),
                                     MakeChip(L"CLOSE", glyphs::Menu(glyphs::B), L"ESC", ChipAct::Close, "Keeps the changes and closes the page.")});
    const float footTop = g_chips[0].y - 0.006f * H;
    // the description: inside the plate, under the rows, in the same place on every tab
    l.descH = 0.030f * H * k * 1.28f * 2 + 0.016f * H;
    l.descTop = footTop - l.descH;
    l.rowsTop = g_card ? stripTop + 0.010f * H : std::max(stripTop + l.tabH + 0.008f * H, closeFoot);
    l.rowsBottom = l.descTop;
    if (g_card || TabIs("subtitles"))
    {
        // the sample stands under the head, always, and as tall as it needs; the card has no head, so it stands first
        l.previewAfter = g_card ? 0 : 1;
        const float rows = l.rowsBottom - l.rowsTop - (g_card ? 4 : 5) * l.pitch;
        l.previewH = std::clamp(g_previewNeedH, 0.092f * H * k, std::max(0.092f * H * k, rows - 0.008f * H));
        l.previewTop = l.rowsTop + l.previewAfter * l.pitch;
        l.previewGap = l.previewH + 0.008f * H;
    }
    l.visible = std::max(1, static_cast<int>((l.rowsBottom - l.rowsTop - l.previewGap) / l.pitch));
    return l;
}

// Over the page: a dialog's two answers, the layout card's foot. Fills g_chips; the page's own chips do not answer while it stands.
static void ModalChips(Layout& l)
{
    if (g_modal == Modal::None)
        return;
    g_behind = g_chips;
    g_chips.clear();
    const float H = l.sh, k = l.k;
    if (g_modal == Modal::Layout)
    {
        Layout c = l;
        const float halfW = std::min(0.68f * H, l.sw * 0.49f);
        c.left = l.cx - halfW, c.right = l.cx + halfW;
        FootChips(c, 0.975f * H, {MakeChip(L"PREVIOUS LAYOUT", glyphs::LB, L"Q", ChipAct::LayoutPrev, ""), MakeChip(L"NEXT LAYOUT", glyphs::RB, L"E", ChipAct::LayoutNext, ""),
                                  MakeChip(L"CLOSE", glyphs::Menu(glyphs::B), L"ESC", ChipAct::Close, "")});
        return;
    }
    const float w = std::min(1.02f * H, l.sw * 0.96f), hh = (0.300f + 0.2f * std::max(0.0f, k - 1)) * H;
    const float x0 = l.cx - w * 0.5f, y0 = H * 0.5f - hh * 0.5f, bw = w * 0.5f - 0.066f * H, bh = 0.062f * H * k, by = y0 + hh - bh - 0.032f * H;
    const bool reset = g_modal == Modal::ResetEverything;
    Chip act = MakeChip(reset ? L"RESET EVERYTHING" : L"DISABLE IT", glyphs::COUNT, L"", ChipAct::Confirm, "");
    Chip safe = MakeChip(reset ? L"KEEP MY SETTINGS" : L"KEEP IT ON", glyphs::COUNT, L"", ChipAct::Refuse, "");
    act.x = x0 + 0.050f * H, act.y = by, act.w = bw, act.h = bh;
    safe.x = x0 + w - 0.050f * H - bw, safe.y = by, safe.w = bw, safe.h = bh;
    g_chips.push_back(act);
    g_chips.push_back(safe);
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
    g.y = g_layout.rowsTop + visibleIndex * g_layout.pitch + (visibleIndex >= g_layout.previewAfter ? g_layout.previewGap : 0);
    g.h = g_layout.rowH;
    g.labelX = g_layout.labelX;
    g.controlLeft = g_layout.controlLeft;
    g.controlRight = g_layout.controlRight;
    g.sh = g_layout.sh;
    return g;
}

// ------------------------------------------------------------------ actions
// The list loops within its rows: the chips answer to their own buttons, so nobody walks past a page of rows to reach one.
static void MoveFocus(int dir)
{
    const int n = static_cast<int>(g_entries.size());
    int f = g_focus;
    for (int tries = 0; tries < n; ++tries)
    {
        f += dir;
        if (f < 0)
            f = n - 1;
        if (f >= n)
            f = 0;
        if (Interactive(f))
            break;
    }
    g_focus = f;
    g_capturing = false;
    KeepFocusVisible();
}

static void SwitchTab(int tab)
{
    const int n = uimeta::kTabCount;
    g_tabFocus[g_tab] = g_focus;
    g_tab = ((tab % n) + n) % n;
    g_focus = g_tabFocus[g_tab];
    g_scroll = 0;
    g_capturing = false;
    BuildEntries();
    SettleFocus();
}

static void ShowLayout(int step)
{
    g_layoutShown = (g_layoutShown + step + uimeta::kBindingPresetCount) % uimeta::kBindingPresetCount;
}

static void PressChip(ChipAct act)
{
    switch (act)
    {
    case ChipAct::Default: ResetTab(); break;
    case ChipAct::Cancel:
        RevertAll();
        CloseAs(false); // hosted: the game's page comes back; standalone: the panel goes
        break;
    case ChipAct::Close:
        if (g_modal == Modal::Layout)
            g_modal = Modal::None;
        else
            CloseAs(true);
        break;
    case ChipAct::LayoutPrev: ShowLayout(-1); break;
    case ChipAct::LayoutNext: ShowLayout(1); break;
    case ChipAct::Confirm:
    {
        const Modal was = g_modal;
        g_modal = Modal::None;
        if (was == Modal::ResetEverything)
            ResetEverything();
        else if (const uimeta::Row* head = uimeta::FindRow("mod_controller"))
            ActHead(*head, false, -1, true);
        break;
    }
    case ChipAct::Refuse: g_modal = Modal::None; break;
    }
}

// A head: left / right step its words, A flips it; a stopped one takes nothing.
static void ActOnHead(const Entry& e, int dir)
{
    const uimeta::Row& m = *e.meta;
    const Head state = HeadStateOf(m);
    if (!optrules::Takes(state))
        return;
    const bool now = state == Head::Enabled || state == Head::EnabledAtNextStart; // what the row reads
    if (m.kind == uimeta::Picker) // HUD 2: three words, the last is off
    {
        const int n = m.optionCount, i = now ? std::max(0, PickerIndex(e)) : n - 1, next = (i + (dir < 0 ? -1 : 1) + n) % n;
        ActHead(m, next != n - 1, next, false);
        return;
    }
    const bool want = dir == 0 ? !now : dir > 0;
    if (want != now)
        ActHead(m, want, -1, false);
}

static void OpenLayoutCard()
{
    g_modal = Modal::Layout;
    g_layoutShown = std::clamp(atoi(Get("Controller", "pad_preset").c_str()), 0, uimeta::kBindingPresetCount - 1);
}

// left / right on a row (dir -1 / +1), accept (dir 0)
static void ActOnRow(Entry& e, int dir)
{
    if (e.kind == Entry::Row && e.meta->head)
    {
        ActOnHead(e, dir);
        return;
    }
    if (Dimmed(e))
    {
        // the second signal: nothing to change here, A or Enter goes where it can change
        if (dir == 0 && e.head)
            for (size_t i = 0; i < g_entries.size(); ++i)
                if (g_entries[i].kind == Entry::Row && !strcmp(g_entries[i].meta->key, e.head))
                {
                    g_focus = static_cast<int>(i);
                    KeepFocusVisible();
                    break;
                }
        return;
    }
    if (ParentOff(e))
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
        if (dir == 0 && !strcmp(m.key, "pad_preset")) // A shows every button of the layout
        {
            OpenLayoutCard();
            break;
        }
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
            {
                g_modal = Modal::ResetEverything;
                g_modalFocus = 1;
            }
            else if (strcmp(m.key, "other_changed") == 0)
                ResetOthers();
            else if (strcmp(m.key, "about") == 0)
                Toast("FLvibed " FLV_VERSION " (built " __DATE__ "). Freelancer (c) Microsoft; subtitle data GPL-3.0; credits in the README. "
                      "Settings: FLController.ini, FLSubtitles.ini and FLvibed.ini in the EXE folder.");
            else if (strcmp(m.key, "edit_bindings") == 0)
                OpenLayoutCard();
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
// What is under the pointer: an entry index, -2 - chip, -10 - tab, -20 .. -24 the scroll bar.
static int HitChip(float px, float py)
{
    for (size_t i = 0; i < g_chips.size(); ++i)
    {
        const Chip& c = g_chips[i];
        if (px >= c.x && px <= c.x + c.w && py >= c.y && py <= c.y + c.h)
            return -2 - static_cast<int>(i);
    }
    return -1;
}

// The visible row at a height, the sample's gap left out; -1 none.
static int RowAt(float py)
{
    const Layout& l = g_layout;
    float rel = py - l.rowsTop;
    if (rel < 0)
        return -1;
    const float gapAt = l.previewAfter * l.pitch;
    if (rel >= gapAt)
    {
        if (rel < gapAt + l.previewGap)
            return -1;
        rel -= l.previewGap;
    }
    const int vi = static_cast<int>(rel / l.pitch);
    return vi < l.visible ? vi : -1;
}

static int HitTest(float px, float py)
{
    const Layout& l = g_layout;
    const int n = static_cast<int>(g_entries.size());
    if (g_modal != Modal::None) // only what stands over the page answers
        return HitChip(px, py);
    if (g_sb.on && std::fabs(px - g_sb.x) <= g_sb.a * 1.3f) // the scroll bar (before the rows: it may overlap them)
    {
        if (std::fabs(py - g_sb.upY) <= g_sb.a * 1.3f)
            return -20;
        if (std::fabs(py - g_sb.downY) <= g_sb.a * 1.3f)
            return -21;
        if (py >= g_sb.trackTop && py <= g_sb.trackTop + g_sb.trackH)
            return py < g_sb.thumbY ? -22 : py > g_sb.thumbY + g_sb.thumbH ? -23 : -24;
    }
    if (px >= l.left && px <= l.right && py >= l.rowsTop && py < l.rowsBottom)
    {
        const int vi = RowAt(py), i = vi < 0 ? -1 : g_scroll + vi;
        return i >= 0 && i < n && Interactive(i) ? i : -1;
    }
    const int chip = HitChip(px, py);
    if (chip != -1)
        return chip;
    if (!g_card && std::fabs(py - l.tabsY) <= l.tabH * 0.5f)
        for (int t = 0; t < uimeta::kTabCount; ++t)
            if (px >= l.tabX[t] && px <= l.tabX[t] + l.tabW[t])
                return -10 - t;
    return -1;
}

static void ClickRow(Entry& e, int visibleIndex, float px, bool firstClick)
{
    if ((e.kind == Entry::Row && e.meta->head) || Dimmed(e)) // a head flips, a dimmed row goes to its head
    {
        if (firstClick)
            ActOnRow(e, 0);
        return;
    }
    if (ParentOff(e))
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
                ActOnRow(e, !strcmp(e.meta->key, "pad_preset") ? 0 : 1); // (the mouse's way to the layout card: the row's label)
        }
        break;
    default:
        if (firstClick)
            ActOnRow(e, 0);
        break;
    }
}

// ------------------------------------------------------------------ per frame
// What stands over the page keeps the keys and the pad: a dialog's two answers, the safe one focused and B or Esc its way out; the layout card's chips.
static void ModalInput(const uikit::UiInput& in)
{
    if (g_modal == Modal::Layout)
    {
        if (in.tabPrev || in.left)
            ShowLayout(-1);
        if (in.tabNext || in.right)
            ShowLayout(1);
        if (in.back || in.accept)
            g_modal = Modal::None;
        return;
    }
    if (in.left)
        g_modalFocus = 0;
    if (in.right)
        g_modalFocus = 1;
    if (in.back)
        g_modal = Modal::None;
    else if (in.accept)
        PressChip(g_modalFocus == 0 ? ChipAct::Confirm : ChipAct::Refuse);
}

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
    const uikit::Input::PadExtra pad = g_input.Extra();
    const bool wasModal = g_modal != Modal::None;
    if (wasModal)
        ModalInput(in);
    else if (in.back)
    {
        if (g_capturing)
            g_capturing = false;
        else
            CloseAs(true); // hosted: back to the game's page (the host keeps the screen open)
        return;
    }
    else if (!g_capturing && (in.defaultKey || pad.x))
        PressChip(ChipAct::Default);
    else if (!g_capturing && (in.cancelKey || pad.y) && !g_card)
    {
        PressChip(ChipAct::Cancel);
        return;
    }
    else
    {
        if (!g_card && in.tabPrev)
            SwitchTab(g_tab - 1);
        if (!g_card && in.tabNext)
            SwitchTab(g_tab + 1);
        if (in.up)
            MoveFocus(-1);
        if (in.down)
            MoveFocus(+1);
        if (in.pageUp)
        {
            g_focus = std::max(0, g_focus - g_layout.visible);
            SettleFocus();
            KeepFocusVisible();
        }
        if (in.pageDown)
        {
            g_focus = std::min(n - 1, g_focus + g_layout.visible);
            while (g_focus > 0 && !Interactive(g_focus))
                --g_focus;
            KeepFocusVisible();
        }
        if (g_focus < static_cast<int>(g_entries.size()))
        {
            if (in.left)
                ActOnRow(g_entries[g_focus], -1);
            if (in.right)
                ActOnRow(g_entries[g_focus], +1);
            if (in.accept)
                ActOnRow(g_entries[g_focus], 0);
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
        if (g_modal != Modal::None)
        {
            if (hit <= -2 && (g_modal != Modal::Layout) && moved)
                g_modalFocus = -2 - hit;
            if (in.click && hit <= -2 && -2 - hit < static_cast<int>(g_chips.size()))
                PressChip(g_chips[-2 - hit].act);
            return;
        }
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
                PressChip(g_chips[-2 - hit].act);
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
        else if (p == "x" || p == "d") in.defaultKey = true;
        else if (p == "y" || p == "c") in.cancelKey = true;
        in.anyKey = true;
    }
    g_pressQueue.clear();
}

void Update(double dtd)
{
    const float dt = std::clamp(static_cast<float>(dtd), 0.0f, 0.25f);
    // the host's page switch: the Options screen on FLvibed's page opens the hosted page, leaving it closes it
    const int state = hostcalls::g.OptionsPageState ? hostcalls::g.OptionsPageState() : 0;
    if (state == 2 && !g_open)
        OpenAs(Host::Options);
    else if (state != 2 && g_open && g_hostMode == Host::Options)
        CloseAs(true);
    // the page button's state while the Options screen is open: shown, lit (the selected page's yellow) on our page
    if (state != 0 && hostcalls::g.SetUiButton)
        hostcalls::g.SetUiButton(kUiBtnOptionsPage, true, state == 2, true);
    // the first-start card: once, at the first main menu of a fresh install, after the startup movies; stamped as it opens, so a crash or a quit never shows it twice
    if (!g_cardChecked && !g_open && session::MainMenuVisible())
    {
        g_cardChecked = true;
        const std::string dir = DllDir().c_str();
        if (settings_reset::FirstCardFor(settings_reset::CardStamped(dir.c_str()), false) == settings_reset::FirstCard::Show)
        {
            settings_reset::StampCard(dir.c_str());
            g_card = true;
            OpenAs(Host::Standalone);
        }
    }
    if (!g_open)
    {
        g_pressQueue.clear();
        if (hostcalls::g.RealMouseWheel && !navview::Movable() && !views::HudPageOn()) // (the nav map's wheel is the map's, a NeuroNet page's the page's)
            hostcalls::g.RealMouseWheel(); // drained while closed: no old notches on the next open
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
            SettleFocus();
        // its rows land on every tab, and few on MORE where the switch is: say so
        if (g_lastAdvanced != wasAdvanced)
            Toast(g_lastAdvanced ? "All settings shown: more rows on every tab." : "Only the main settings shown.");
    }
    g_layout = ComputeLayout();
    ModalChips(g_layout);
    KeepFocusVisible();
    uikit::UiInput in = g_input.Poll(dt, g_capturing);
    ApplyDebugPresses(in);
    HandleInput(in, dt);
    // the mouse wheel: 3 rows a notch (+ = up), the list's own as in the game's lists
    const int wheel = hostcalls::g.RealMouseWheel ? hostcalls::g.RealMouseWheel() : 0;
    if (wheel && g_open && !g_capturing)
        ScrollBy(-wheel * 3);
    // the prompt pills (FIX2: every menu shows at least A and B; this page owns the pad while open, so padmenus' Options
    // prompts do not run here): a key row waiting for its key offers only B CANCEL
    if (g_open)
    {
        // the game's arrow follows the pad cursor rule here too (padbases decides; the pad's menu code does not run
        // while this page owns the pad - the arrow stood over the page's tabs; keyboard and mouse get it back there)
        views::NoteFrame(true);
        using prompts::Prompt;
        if (g_capturing)
        {
            static const Prompt p[] = {{glyphs::B, L"CANCEL"}};
            views::SetPrompts("options.flvibed.key", p, 1, true, 10);
        }
        // (else nothing: the chips carry the page's buttons)
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
    std::ifstream in(std::string(DllDir().c_str()) + "FLSubtitles\\speaker_colors.tsv");
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
    // the band: a dark space-like backing
    render::Box(l.left + sh * 0.02f, l.previewTop, l.right - l.left - sh * 0.04f, l.previewH, 0xC0050812);
    uidraw::Frame(l.left + sh * 0.02f, l.previewTop, l.right - l.left - sh * 0.04f, l.previewH,
                  std::max(1.0f, sh * 0.001f), uidraw::Fade(uidraw::kFrame, 0.5f));
    if (!SubtitlesHead() || !optrules::RowsLive(HeadStateOf(*SubtitlesHead())))
    {
        render::SetFont(f.body);
        const std::wstring off = L"No subtitles";
        render::Text((l.left + l.right - render::TextWidth(f.body, off)) * 0.5f, l.previewTop + (l.previewH - f.body.lineHeight) * 0.5f, off, uidraw::kDisabled);
        g_previewNeedH = 0;
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
    const std::wstring sample = L"Trent, this is Juni. Meet me at the bar.";
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
    g_previewNeedH = h + 2 * padY + sh * 0.016f; // the next layout sizes the band to it (its margins)
    // centred on the band (the hosted panel sits right of the screen's centre)
    const float cx = (l.left + l.right) * 0.5f, y0 = l.previewTop + (l.previewH - h) * 0.5f;
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
// (uibtn.cpp kNativeOptionsCaption). The sibling captions' style, measured on options_general-003103.png
// at 4K: Agency FB Regular with the caps 56 px tall ("GENERAL" 188 px wide, "MAIN MENU" 239) = 72 px em, 0.0333 H;
// centred on the button; the normal light blue (uidraw::kText), the selected page's yellow while FLvibed's page shows
// (OptionsPageState 2); the game's dark shadow down-right.
static render::Font g_capFont;

static void FreeCaption() { render::FreeFont(g_capFont); }

static void DrawCaption()
{
    if (!hostcalls::g.UiButtonRect || !hostcalls::g.OptionsPageState || !hostcalls::g.GetD3DDevice)
        return;
    const int state = hostcalls::g.OptionsPageState();
    if (state == 0)
        return;
    float cx = 0, cy = 0, hw = 0, hh = 0;
    if (!hostcalls::g.UiButtonRect(kUiBtnOptionsPage, &cx, &cy, &hw, &hh) || hh <= 1)
        return;
    void* device = hostcalls::g.GetD3DDevice();
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

// The spec's words with a button or the version in them, in the live family's names.
static std::string Expand(const char* text, bool upper)
{
    std::string s = optrules::ExpandButtons(text ? text : "", static_cast<int>(glyphs::Current()), views::DevicePad(), upper);
    const size_t at = s.find("{version}");
    if (at != std::string::npos)
        s.replace(at, 9, FLV_VERSION);
    return s;
}

static std::string RowHelp(const Entry& e)
{
    if (e.kind == Entry::Row && e.meta->head && HeadStateOf(*e.meta) == Head::Stopped)
        return "It stopped after a fault. Restart the game.";
    if (Dimmed(e))
    {
        const std::string why = DimHelp(e);
        if (!why.empty())
            return why;
    }
    if (e.kind == Entry::Row && e.meta->kind == uimeta::Picker && e.meta->valueHelp)
    {
        const int i = PickerIndex(e);
        if (i >= 0 && i < e.meta->optionCount && e.meta->valueHelp[i])
            return Expand(e.meta->valueHelp[i], false);
    }
    return Expand(e.help, false);
}

// The description: inside the plate, under the rows, in the same place on every tab, what the focused row or the chip under the pointer does.
static void DrawDescription(const uikit::Fonts& f)
{
    const Layout& l = g_layout;
    const float H = l.sh;
    std::string text;
    if (g_toastTime > 0)
        text = g_toast;
    else if (g_hover <= -2 && g_hover > -10 && -2 - g_hover < static_cast<int>(g_chips.size()))
        text = g_chips[-2 - g_hover].help;
    else if (g_hover >= 0 && g_hover < static_cast<int>(g_entries.size()))
        text = RowHelp(g_entries[g_hover]);
    else if (g_focus < static_cast<int>(g_entries.size()))
        text = RowHelp(g_entries[g_focus]);
    render::Box(l.left + 0.018f * H, l.descTop, l.right - l.left - 0.036f * H, std::max(2.0f, H * 0.0012f), uidraw::Fade(uidraw::kFrame, 0.5f));
    if (text.empty())
        return;
    render::SetFont(f.body);
    const float lh = f.body.lineHeight * 1.05f, x = l.left + 0.026f * H;
    const std::vector<std::wstring> lines = Wrap(f.body, Widen(text.c_str()), l.right - l.left - 0.052f * H, 2);
    for (size_t i = 0; i < lines.size(); ++i)
        render::Text(x, l.descTop + 0.008f * H + i * lh, lines[i], uidraw::kText);
}

// A head's row: its label, and its value in the words of its state; the wait in amber, on the row itself.
static void DrawHead(const uikit::Fonts& f, const uikit::RowGeom& g, const uikit::RowStyle& s, const Entry& e, bool focused)
{
    uikit::RowStyle label = s;
    label.enabled = true;
    uikit::RowLabel(f, g, label, e.label);
    const Head state = HeadStateOf(*e.meta);
    const u32 amber = hud2kit::Argb(hud2kit::kAmber, 1);
    const u32 ink = focused ? uidraw::kSelected : uidraw::kText;
    std::wstring word = L"ENABLED";
    if (e.meta->kind == uimeta::Picker && state == Head::Enabled)
    {
        const int i = PickerIndex(e);
        if (i >= 0)
            word = Widen(e.meta->options[i]);
    }
    switch (state)
    {
    case Head::Enabled: uikit::RowHead(f, g, word, ink, true, true); break;
    case Head::Disabled: uikit::RowHead(f, g, L"DISABLED", ink, true, false); break;
    case Head::DisabledAtNextStart: uikit::RowHead(f, g, L"DISABLED AT NEXT START", amber, true, false); break;
    case Head::EnabledAtNextStart: uikit::RowHead(f, g, L"ENABLED AT NEXT START", amber, true, true); break;
    case Head::Stopped: uikit::RowHead(f, g, L"STOPPED", amber, false, false); break;
    }
    if (!g_card && !TabIs("more")) // the head stands apart from the rows it switches
        render::Box(g.labelX - g.sh * 0.01f, g.y + g.h + g.sh * 0.001f, g.controlRight - g.labelX + g.sh * 0.02f, std::max(1.0f, g.sh * 0.0008f), uidraw::Fade(uidraw::kFrame, 0.43f));
}

static void DrawEntry(const uikit::Fonts& f, Entry& e, int visibleIndex, bool focused)
{
    const uikit::RowGeom g = GeomOf(visibleIndex);
    if (e.kind == Entry::Header)
    {
        render::SetFont(f.title);
        uidraw::TextShadow(g.labelX - g.sh * 0.010f, g.y + (g.h - f.title.lineHeight) * 0.5f, e.label, uidraw::kText,
                           std::max(1.0f, g.sh * 0.001f));
        return;
    }
    const bool skin = views::SkinOn();
    const bool dim = Dimmed(e);
    uikit::RowStyle s;
    s.focused = focused && !skin; // HUD 2: the one focus style is drawn by DrawHud2Back
    s.lit = focused && skin;
    s.ownCase = skin; // sentence case on the skin's plate; the game's look keeps the game's capitals
    s.enabled = !dim && !ParentOff(e);
    s.bare = dim;
    s.changed = Changed(e);
    s.indent = e.indent ? g.sh * 0.026f : 0;
    if (e.kind == Entry::Row && e.meta->head)
    {
        DrawHead(f, g, s, e, focused);
        return;
    }
    if (e.kind == Entry::Row)
    {
        if (e.meta->needs == uimeta::Load)
            s.note = L"AT THE NEXT LOAD";
        // the FLUF row says whether FLUF is there and whether FLvibed reads its windows (docs\MOD_COMPATIBILITY.md FLUF)
        if (e.meta->key && strcmp(e.meta->key, "fluf_windows") == 0)
            s.note = !flufaware::Present()               ? L"NO FLUF INSTALLED"
                     : flufaware::State().present == 2   ? L"FLUF DETECTED"
                                                         : L"FLUF DETECTED - CO-RESIDENCY ONLY";
        // the borderless row says who provides it at this start (the Display module, through hostcalls; the setting acts at the next)
        if (e.meta->key && strcmp(e.meta->key, "borderless") == 0)
        {
            const char* p = hostcalls::g.WindowMode ? hostcalls::g.WindowMode(nullptr, 0) : "off";
            s.note = !strcmp(p, "flvibed")      ? L"BY FLVIBED"
                     : !strcmp(p, "game")       ? L"BY THE GAME (HDE)"
                     : !strcmp(p, "wrapper")    ? L"BY THE DIRECT3D WRAPPER"
                     : !strcmp(p, "unverified") ? L"NOT AVAILABLE - THE GAME'S MODE"
                     : !strcmp(p, "off")        ? L"OFF"
                                                : L"AFTER RESTART";
            if (s.changed)
                s.note = L"AFTER RESTART"; // (the pick acts at the next start: the provider above is this start's)
        }
        if (dim)
            s.note = nullptr;
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
        if (i >= 0)
            word = Widen(Expand(m.options[i], true).c_str());
        else if (!v.empty())
            word = Widen(v.c_str());
        else
            word = L"NOT AVAILABLE";
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

// The chips the page itself shows: under a dialog they stand behind the scrim.
static const std::vector<Chip>& PageChips() { return g_modal == Modal::None ? g_chips : g_behind; }

// Something on this tab waits for the next start: the amber mark shows it from any tab.
static bool TabWaits(int tab)
{
    const uimeta::Tab& t = uimeta::kTabs[tab];
    for (int s = 0; s < t.count; ++s)
        for (int r = 0; r < t.sections[s].count; ++r)
            if (t.sections[s].rows[r].head && optrules::Waits(HeadStateOf(t.sections[s].rows[r])))
                return true;
    return false;
}

static bool KitBegin(int& w, int& h)
{
    void* device = hostcalls::g.GetD3DDevice ? hostcalls::g.GetD3DDevice() : nullptr;
    if (!device || !render::ScreenSize(w, h) || h < 100)
        return false;
    if (!hud2kit::Begin(device, w, h, std::max(8, static_cast<int>(0.026f * h * g_layout.k + 0.5f))))
        return false;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    return true;
}

// A tab or chip plate of the skin; the current tab carries the yellow underline.
static void Hud2Plate(float x, float y, float tw, float th, bool current, bool lit)
{
    const float H = g_layout.sh;
    hud2kit::Plate(x, y, x + tw, y + th, H * 0.010f, hud2kit::kTL | hud2kit::kBR, 220 / 255.0f, hud2kit::kChrome, (current || lit ? 200 : 120) / 255.0f);
    if (current)
        hud2kit::Line(x + H * 0.012f, y + th - H * 0.004f, x + tw - H * 0.012f, y + th - H * 0.004f, H * 0.004f, hud2kit::Argb(hud2kit::kActive, 1));
}

// ui_style HUD 2 (modules\hud\docs\UI_HUD2_EXTENDED.md 10; the mock tools\uimock\options.py): the page's chrome in HUD 2's kit, under the texts the classic pass draws after it -
// the tab and chip plates (no purple fill; the current tab with the yellow underline), the focused row's fill at 28 % active yellow with the bar outside its left edge, the
// standalone panel as one glass plate over a scrim.
static void DrawHud2Back(const Layout& l, int n)
{
    int w = 0, h = 0;
    if (!KitBegin(w, h))
        return;
    const float H = l.sh;
    // the standalone panel fully opaque (fix-text 3: at 0.85 the room behind - a character's name label "Trent" - showed
    // through the rows); hosted, the Options screen's own window is the backing
    if (g_hostMode == Host::Standalone)
    {
        hud2kit::Rect(0, 0, static_cast<float>(w), static_cast<float>(h), hud2kit::Argb(0, 0.70f));
        hud2kit::Plate(l.left, l.panelTop, l.right, l.panelBottom, H * 0.012f, hud2kit::kTL | hud2kit::kBR, 1.0f, hud2kit::kChrome, 175 / 255.0f);
    }
    if (!g_card)
        for (int t = 0; t < uimeta::kTabCount; ++t)
            Hud2Plate(l.tabX[t], l.tabsY - l.tabH * 0.5f, l.tabW[t], l.tabH, t == g_tab, g_hover == -10 - t);
    const std::vector<Chip>& chips = PageChips();
    for (size_t i = 0; i < chips.size(); ++i)
        Hud2Plate(chips[i].x, chips[i].y, chips[i].w, chips[i].h, false, g_modal == Modal::None && g_hover == -2 - static_cast<int>(i));
    // the focus: its label to the control's right end, as the classic frame
    if (g_focus >= 0 && g_focus < n && g_focus >= g_scroll && g_focus - g_scroll < l.visible && g_entries[g_focus].kind != Entry::Header)
    {
        const uikit::RowGeom g = GeomOf(g_focus - g_scroll);
        const float fx0 = g.labelX - H * 0.012f, fx1 = g.controlRight + H * 0.012f;
        hud2kit::Rect(fx0, g.y, fx1 - fx0, g.h, hud2kit::Argb(hud2kit::kActive, 0.28f));
        hud2kit::Rect(fx0 - H * 0.008f, g.y, H * 0.005f, g.h, hud2kit::Argb(hud2kit::kActive, 1));
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
    float r[4];
    int w = 0, h = 0;
    if (!HostedPlate(r) || !render::ScreenSize(w, h) || h < 100)
        return;
    const float H = static_cast<float>(h);
    if (!hud2kit::Begin(device, w, h, std::max(8, static_cast<int>(0.026f * H + 0.5f))))
        return;
    hud2kit::SetAlpha(1), hud2kit::SetOffset(0, 0);
    hud2kit::Plate(r[0], r[1], r[2], r[3], H * 0.012f, hud2kit::kTL | hud2kit::kBR, 1.0f, hud2kit::kChrome, 0);
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

// A chip's plate in the game's look, and its word in either look: the word moves right of the button that goes with it.
static void DrawChip(const uikit::Fonts& f, const Chip& c, bool lit, bool hud2)
{
    const float sh = g_layout.sh;
    if (!hud2)
        uikit::Button(f, c.x, c.y, c.w, c.h, sh, L"", lit, true);
    const float shift = c.pad != glyphs::COUNT ? 0.012f * sh * g_layout.k : 0;
    Hud2Caption(f, c.x + shift, c.y, c.w, c.h, sh, c.word, lit);
}

// The buttons beside the words: the live device's only, drawn in one kit pass over the page's text.
static void DrawButtons(const uikit::Fonts& f, const std::vector<Chip>& chips, const std::vector<presets::LayoutLine>* lines, float lineX, float lineY, float pitch)
{
    int w = 0, h = 0;
    if (!KitBegin(w, h))
        return;
    const Layout& l = g_layout;
    const float gh = 0.032f * l.sh * l.k;
    const u32 tint = hud2kit::Argb(hud2kit::kChrome, 1);
    const bool pad = views::DevicePad();
    for (const Chip& c : chips)
    {
        if (c.pad == glyphs::COUNT)
            continue;
        float x = c.x + c.w * 0.5f - render::TextWidth(f.label, c.word) * 0.5f - 0.016f * l.sh * l.k;
        const float y = c.y + c.h * 0.5f;
        if (pad)
            hud2kit::Glyph(c.pad, x, y, gh, tint);
        else
        {
            // (a wide cap stands off the word by its own width: ESC touched CLOSE. The word starts at the caption's shift)
            const float wordX = c.x + 0.012f * l.sh * l.k + c.w * 0.5f - render::TextWidth(f.label, c.word) * 0.5f;
            x = std::min(x, wordX - 0.008f * l.sh * l.k - hud2kit::Keycap(c.key, 0, 0, gh, 0, true) * 0.5f);
            hud2kit::Keycap(c.key, x, y, gh, tint);
        }
    }
    if (!g_card && g_modal == Modal::None)
    {
        const float xl = l.tabsLeft - 0.020f * l.sh * l.k, xr = l.tabsRight + 0.020f * l.sh * l.k;
        if (pad)
            hud2kit::Glyph(glyphs::LB, xl, l.tabsY, gh * 0.875f, tint), hud2kit::Glyph(glyphs::RB, xr, l.tabsY, gh * 0.875f, tint);
        else
            hud2kit::Keycap(L"Q", xl, l.tabsY, gh * 0.875f, tint), hud2kit::Keycap(L"E", xr, l.tabsY, gh * 0.875f, tint);
    }
    if (lines)
        for (size_t i = 0; i < lines->size(); ++i)
            hud2kit::Glyph((*lines)[i].btn, lineX, lineY + (i + 0.5f) * pitch, 0.034f * l.sh * l.k, tint);
    hud2kit::End();
}

// A dialog or the layout card over the page: the scrim, the plate, its words, its answers.
static void DrawModal(const uikit::Fonts& f, const Layout& l, bool hud2)
{
    const float H = l.sh, k = l.k;
    const bool card = g_modal == Modal::Layout;
    const float w = card ? std::min(1.36f * H, l.sw * 0.98f) : std::min(1.02f * H, l.sw * 0.96f);
    const float hh = (0.300f + 0.2f * std::max(0.0f, k - 1)) * H;
    const float x0 = l.cx - w * 0.5f, y0 = card ? 0.035f * H : H * 0.5f - hh * 0.5f, y1 = card ? 0.975f * H : y0 + hh;
    if (hud2)
    {
        int sw = 0, sh = 0;
        if (KitBegin(sw, sh))
        {
            hud2kit::Rect(0, 0, static_cast<float>(sw), static_cast<float>(sh), hud2kit::Argb(0, 0.70f));
            hud2kit::Plate(x0, y0, x0 + w, y1, H * 0.012f, hud2kit::kTL | hud2kit::kBR, 1.0f, hud2kit::kChrome, 175 / 255.0f, card ? 0 : 't', hud2kit::kAmber);
            for (size_t i = 0; i < g_chips.size(); ++i)
            {
                const bool focus = !card && static_cast<int>(i) == g_modalFocus;
                const Chip& c = g_chips[i];
                Hud2Plate(c.x, c.y, c.w, c.h, false, focus || g_hover == -2 - static_cast<int>(i));
                if (focus)
                {
                    hud2kit::Rect(c.x, c.y, c.w, c.h, hud2kit::Argb(hud2kit::kActive, 0.28f));
                    hud2kit::Rect(c.x - H * 0.008f, c.y, H * 0.005f, c.h, hud2kit::Argb(hud2kit::kActive, 1));
                }
            }
            hud2kit::End();
        }
    }
    if (!render::Begin(f.label))
        return;
    if (!hud2)
    {
        render::Box(0, 0, l.sw, l.sh, 0xB3000000);
        uidraw::HudPanel(x0, y0, w, y1 - y0, 1.0f, H, GetTickCount64() / 1000.0);
    }
    const auto centred = [&](const render::Font& font, float y, const std::wstring& s, u32 col)
    {
        render::SetFont(font);
        uidraw::TextShadow(l.cx - render::TextWidth(font, s) * 0.5f, y - font.lineHeight * 0.5f, s, col, std::max(1.0f, H * 0.001f));
    };
    std::vector<presets::LayoutLine> lines;
    float lineY = 0, pitch = 0;
    if (card)
    {
        const presets::Table t = presets::Effective(g_layoutShown, atoi(Get("Controller", "weapon_groups").c_str()), atof(Get("Controller", "y_swap").c_str()) != 0);
        lines = presets::LayoutLines(t);
        centred(f.big, y0 + 0.034f * H * k, L"LAYOUT: " + Widen(uimeta::kBindingPresets[g_layoutShown].name), uidraw::kText);
        render::Box(x0 + 0.018f * H, y0 + 0.072f * H * k, w - 0.036f * H, std::max(2.0f, H * 0.0012f), uidraw::Fade(uidraw::kFrame, 0.5f));
        lineY = y0 + 0.086f * H * k;
        pitch = (g_chips[0].y - 0.006f * H - lineY) / std::max<size_t>(1, lines.size());
        render::SetFont(f.label);
        for (size_t i = 0; i < lines.size(); ++i)
            render::Text(x0 + 0.110f * H, lineY + (i + 0.5f) * pitch - f.label.lineHeight * 0.5f, Widen(lines[i].words.c_str()), uidraw::kText);
    }
    else
    {
        const bool reset = g_modal == Modal::ResetEverything;
        centred(f.big, y0 + 0.052f * H * k, reset ? L"RESET EVERYTHING?" : L"DISABLE THE CONTROLLER?", uidraw::kText);
        centred(f.body, y0 + 0.108f * H * k, reset ? L"Every FLvibed setting goes back to its default, hidden ones too." : L"From the next start the game is keyboard and mouse only.", uidraw::kText);
    }
    for (size_t i = 0; i < g_chips.size(); ++i)
        DrawChip(f, g_chips[i], (!card && static_cast<int>(i) == g_modalFocus) || g_hover == -2 - static_cast<int>(i), hud2);
    render::End(false);
    DrawButtons(f, g_chips, card ? &lines : nullptr, x0 + 0.060f * H, lineY, pitch);
}

void Draw()
{
    DrawCaption(); // the page button's caption whenever the Options screen is open (the page itself only when open)
    if (!g_open || !HostedSettled())
        return;
    if (g_layout.sh <= 0)
    {
        g_layout = ComputeLayout();
        ModalChips(g_layout);
    }
    const Layout& l = g_layout;
    if (hostcalls::g.GetD3DDevice)
        render::SetDevice(hostcalls::g.GetD3DDevice()); // the renderer's device (in menus nothing else sets it)
    const uikit::Fonts& f = uikit::PageFontsFor(static_cast<int>(l.sh), static_cast<int>(l.k * 100 + 0.5f));
    const bool hud2 = views::SkinOn();
    if (hud2 && f.ok)
        DrawHud2Back(l, static_cast<int>(g_entries.size()));
    if (!f.ok || !render::Begin(f.label))
    {
        static u32 warned = 0;
        if (warned++ == 0)
            Log("options page: nothing drawn (fonts %d, device %p)", f.ok ? 1 : 0,
                hostcalls::g.GetD3DDevice ? hostcalls::g.GetD3DDevice() : nullptr);
        return;
    }
    const float sh = l.sh;
    const double now = GetTickCount64() / 1000.0;
    if (g_hostMode == Host::Standalone && !hud2)
    {
        render::Box(0, 0, l.sw, l.sh, 0xB3000000);
        uidraw::HudPanel(l.left, l.panelTop, l.right - l.left, l.panelBottom - l.panelTop, 1.0f, sh, now); // opaque: nothing of the room behind shows through (fix-text 3)
    }
    // the title: not on the hosted page, where the lit FLVIBED button in the game's column names it
    if (l.titleY > 0)
    {
        render::SetFont(f.big);
        const std::wstring title = g_card ? Widen(uimeta::kCardTitle) : L"FLVIBED OPTIONS";
        uidraw::TextShadow(l.titleX - render::TextWidth(f.big, title) * 0.5f, l.titleY - f.big.lineHeight * 0.5f, title, uidraw::kText, std::max(1.0f, sh * 0.001f));
    }
    if (g_card)
    {
        render::SetFont(f.body);
        const std::wstring lead = Widen(uimeta::kCardLead);
        render::Text(l.titleX - render::TextWidth(f.body, lead) * 0.5f, l.panelTop + 0.076f * sh * l.k - f.body.lineHeight * 0.5f, lead, uidraw::kText);
    }
    // the tabs, an amber mark on one that waits for the next start
    for (int t = 0; t < uimeta::kTabCount && !g_card; ++t)
    {
        const float ty = l.tabsY - l.tabH * 0.5f;
        if (hud2)
            Hud2Caption(f, l.tabX[t], ty, l.tabW[t], l.tabH, sh, Widen(uimeta::kTabs[t].label), t == g_tab);
        else
            uikit::TabButton(f, l.tabX[t], ty, l.tabW[t], l.tabH, sh, Widen(uimeta::kTabs[t].label), t == g_tab, g_hover == -10 - t);
        if (TabWaits(t))
            uidraw::Dot(l.tabX[t] + l.tabW[t] - 0.012f * sh, ty + 0.012f * sh, 0.006f * sh * l.k, hud2kit::Argb(hud2kit::kAmber, 1));
    }
    // the rows
    const int n = static_cast<int>(g_entries.size());
    for (int i = g_scroll, vi = 0; i < n && vi < l.visible; ++i, ++vi)
        DrawEntry(f, g_entries[i], vi, i == g_focus);
    g_sb.on = n > l.visible;
    if (n > l.visible) // the scroll arrows at the right, as the Controls page's table has them
    {
        const float ax = l.controlRight + sh * 0.020f, a = sh * 0.012f, listH = l.visible * l.pitch + l.previewGap;
        uidraw::Triangle(ax, l.rowsTop + a, a, 0, g_scroll > 0 ? uidraw::kText : uidraw::kDisabled);
        uidraw::Triangle(ax, l.rowsTop + listH - a, a, 1,
                         g_scroll + l.visible < n ? uidraw::kText : uidraw::kDisabled);
        // the thumb between them
        const float trackTop = l.rowsTop + 2.5f * a, trackH = listH - 5.0f * a;
        const float th = std::max(a, trackH * l.visible / n), ty = trackTop + (trackH - th) * g_scroll / std::max(1, n - l.visible);
        g_sb.x = ax, g_sb.a = a, g_sb.upY = l.rowsTop + a, g_sb.downY = l.rowsTop + listH - a;
        g_sb.trackTop = trackTop, g_sb.trackH = trackH, g_sb.thumbY = ty, g_sb.thumbH = th;
        render::Box(ax - a * 0.25f, trackTop, a * 0.5f, trackH, uidraw::Fade(uidraw::kTrack, 0.4f));
        render::Box(ax - a * 0.4f, ty, a * 0.8f, th, uidraw::kThumb);
    }
    if (l.previewH > 0)
        DrawPreview(f);
    // the chips, the description
    const std::vector<Chip>& chips = PageChips();
    for (size_t i = 0; i < chips.size(); ++i)
        DrawChip(f, chips[i], g_modal == Modal::None && g_hover == -2 - static_cast<int>(i), hud2);
    DrawDescription(f);
    render::End(false);
    DrawButtons(f, chips, nullptr, 0, 0, 0);
    if (g_modal != Modal::None)
        DrawModal(f, l, hud2);
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
        if (hostcalls::g.OptionsPageState && hostcalls::g.OptionsPageState() == 1 && hostcalls::g.ShowOptionsPage)
            hostcalls::g.ShowOptionsPage(true);
    }
}

bool OnRawKey(int vk, bool down, bool textField)
{
    if (!g_open || textField)
        return false;
    return g_input.PushKey(vk, down, g_capturing); // (Esc too, hosted: CLOSE goes back to the game's Options column, as B does)
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
            if (!e.offNote.empty())
                o << " note=" << uikit::Narrow(e.offNote);
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
        if (e.kind == Entry::Row && e.meta->head)
        {
            static const char* const kStates[] = {"enabled", "disabled", "disabled at next start", "enabled at next start", "stopped"};
            o << " [" << kStates[static_cast<int>(HeadStateOf(*e.meta))] << "]";
        }
        else if (Dimmed(e))
            o << " [dimmed]";
        else if (ParentOff(e))
            o << " [parent off]";
        o << "\n";
    }
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
            if (!OpenBest())
                return "hosting"; // the Options screen switches to FLvibed's page; Update opens it hosted
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
            return "tabs: 0 hud, 1 controller, 2 subtitles, 3 autopilot, 4 more";
        SwitchTab(t);
        return std::string("tab ") + uimeta::kTabs[g_tab].label;
    }
    if (sub == "rows")
        return g_open ? RowsReport() : "closed";
    if (sub == "card") // ui card: the first-start card, as a fresh install meets it; the stamp stays as it is
    {
        if (g_open)
            return "open";
        g_card = true;
        OpenAs(Host::Standalone);
        return "card";
    }
    if (sub == "press") // ui press up|down|left|right|a|b|x|y|lb|rb|pgup|pgdn [...]: the next frame's input
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
          << (hostcalls::g.OptionsPageState ? hostcalls::g.OptionsPageState() : -1)
          << " pad=" << (g_input.PadConnected() ? 1 : 0) << " screen=" << g_layout.sw << "x" << g_layout.sh
          << " tabs=" << uimeta::kTabCount << " settled=" << (g_settled ? 1 : 0) << " text=" << TextPercent()
          << " panel=" << g_layout.left << ".." << g_layout.right << " cx=" << g_layout.cx << " tabs=" << g_layout.tabsLeft << ".." << g_layout.tabsRight
          << " label=" << g_layout.labelX << " control=" << g_layout.controlLeft << ".." << g_layout.controlRight
          << " pitch=" << g_layout.pitch << " k=" << g_layout.k << " desc=" << g_layout.descTop << " card=" << (g_card ? 1 : 0) << " modal=" << static_cast<int>(g_modal);
        for (const Chip& c : g_chips)
            o << " chip=" << uikit::Narrow(c.word) << "@" << c.x << "," << c.y;
        return o.str();
    }
    return "ui: options [open|close|cancel] | tab <n|name> | rows | press <keys...> | focus <key> | default | card | state";
}
} // namespace options_page
