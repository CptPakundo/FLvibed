// `hud2 lint` (docs\UI_HUD2_PRESETS.md 6 step 3): the layout lint (hud2_lint.h, pure) over the live HUD 2 - the items
// the kit recorded in the last drawn pass (hud2kit::Record: armed by the first `hud2 lint`, every pass after records
// until `hud2 lint off`), the layout, the model and the bindings that `hud2 state` prints. What the HUD 2 state does not
// hold is printed "n/a: <rule>: <why>", never guessed. Zones, budgets and prompt caps are the active preset's data
// (`hud_preset`, step 4; the header line names it and whether its layout is built), the value ids' declarations (second
// channels) the shared table (step 1: docs\UI_SPEC.json "hud2_presets" -> core\ui_meta.h uimeta::hud2p); the value ids
// are the model's text fields matched against the drawn strings until the parts tag their values.
#include "hud2_impl.h"
#include "hud2_lint.h"
#include "text.h"
#include "uiopt.h"
#include "wheel.h"

#include <istream>
#include <sstream>

// (ASCII only: the build reads the sources as cp1252, non-ASCII text goes in as \u escapes)
namespace hud2
{
static bool g_lintArmed = false;
static std::vector<hud2kit::Drawn> g_lintRec, g_lintLast; // the pass being recorded, the last whole one
static u32 g_lintTick = 0;                                // when the last whole pass ended (0 none since arming)

void LintPassBegin()
{
    if (!g_lintArmed)
        return;
    g_lintRec.clear();
    hud2kit::Record(&g_lintRec);
}

void LintPassEnd()
{
    if (!g_lintArmed)
        return;
    hud2kit::Record(nullptr);
    g_lintLast.swap(g_lintRec);
    g_lintTick = GetTickCount();
}

static std::wstring Up(const std::wstring& s)
{
    std::wstring u = s;
    for (wchar_t& c : u)
        c = static_cast<wchar_t>(towupper(c));
    return u;
}

// The drawn string shows the field: equal (case aside), or cut by the fit rule (its ellipsis) from it.
static bool Shows(const std::wstring& drawn, const std::wstring& field)
{
    if (field.size() < 3 || drawn.size() < 3)
        return false;
    const std::wstring d = Up(drawn), f = Up(field);
    if (d == f)
        return true;
    return d.size() >= 5 && d.back() == textfit::kEllipsis && f.size() > d.size() - 1 &&
           f.compare(0, d.size() - 1, d, 0, d.size() - 1) == 0;
}

// The model's single-valued text fields by value id (the threat / contact rows are per object and can share a name: not
// matched). Each id must be declared in UI_SPEC.json "hud2_presets" value_ids (ui_meta.h uimeta::hud2p::kValueIds) with
// this field as its `live`; an undeclared one is not matched and printed n/a (fail closed: no value check from a stale id).
struct ValueField
{
    const char* id;
    const char* field; // the Model member's name (= the spec's `live`)
    std::wstring Model::*v;
};
static const ValueField kValueFields[] = {
    {"objective", "objective", &Model::objective}, {"maneuver", "manName", &Model::manName},
    {"maneuver.dest", "manDest", &Model::manDest}, {"eta", "eta", &Model::eta},
    {"ap.mode", "apMode", &Model::apMode},         {"ap.wait", "wait", &Model::wait},
    {"info", "info", &Model::info},                {"event", "event", &Model::event},
    {"place", "place", &Model::place},             {"where", "where", &Model::where},
    {"target.name", "tName", &Model::tName},       {"target.faction", "tFaction", &Model::tFaction},
    {"target.class", "tClass", &Model::tClass},    {"target.distance", "tDist", &Model::tDist},
    {"ship.name", "shipName", &Model::shipName},   {"ship.class", "shipClass", &Model::shipClass},
    {"launcher", "launcher", &Model::launcher},    {"alert", "alert", &Model::alert},
    {"alert.none", "alertNone", &Model::alertNone}};

static const uimeta::hud2p::ValueId* Declared(const ValueField& f)
{
    const uimeta::hud2p::ValueId* v = uimeta::hud2p::FindValueId(f.id);
    return v && v->live && strcmp(v->live, f.field) == 0 ? v : nullptr;
}

// The value id a drawn text shows ("" none) and its declaration.
static const ValueField* ValueOf(const std::wstring& s, const Model& m)
{
    for (const ValueField& e : kValueFields)
        if (Declared(e) && Shows(s, m.*e.v))
            return &e;
    return nullptr;
}

static hud2lint::Kind KindOf(char k)
{
    using K = hud2lint::Kind;
    switch (k)
    {
    case 'P': return K::Plate;
    case 'T': return K::Text;
    case 'G': return K::Glyph;
    case 'K': return K::Keycap;
    case 'A': return K::Arc;
    case 'I': return K::Icon;
    default: return K::Shape;
    }
}

static bool DrawnText(const std::vector<hud2lint::Item>& items, const std::vector<hud2kit::Drawn>& rec, const wchar_t* s)
{
    for (size_t i = 0; i < rec.size(); ++i)
        if (rec[i].kind == 'T' && items[i].alpha > 0 && Up(rec[i].text) == s)
            return true;
    return false;
}

std::string LintCommand(std::istream& in)
{
    std::string v;
    in >> v;
    if (v == "off")
    {
        g_lintArmed = false, g_lintTick = 0;
        hud2kit::Record(nullptr);
        g_lintRec.clear(), g_lintLast.clear();
        return "hud2 lint off";
    }
    if (!g_lintArmed)
    {
        g_lintArmed = true, g_lintTick = 0;
        return "hud2 lint: recording armed (every drawn HUD 2 pass records its items until `hud2 lint off`); run "
               "`hud2 lint` again for the findings";
    }
    if (!g_lintTick)
        return "hud2 lint: n/a: no HUD 2 pass drawn since arming (HUD 2 not visible: `hud2 state`)";

    const Model& m = CurModel();
    const Layout& L = CurLayout();
    const Context& c = CurContext();
    hud2lint::Input li;
    li.W = L.W, li.H = L.H, li.scale = L.k;

    // ---- the items (the kit's records of the last pass)
    li.items.reserve(g_lintLast.size());
    for (const hud2kit::Drawn& d : g_lintLast)
    {
        hud2lint::Item it;
        it.kind = KindOf(d.kind);
        it.x0 = d.x0, it.y0 = d.y0, it.x1 = d.x1, it.y1 = d.y1;
        it.cx = d.cx, it.cy = d.cy, it.r = d.r, it.a0 = d.a0, it.a1 = d.a1, it.w = d.w;
        it.alpha = d.alpha;
        it.unit = d.unit; // (a designed unit: hud2kit::RecUnit - panel.overlap lets it touch)
        it.comp = d.tag ? d.tag : "";
        it.centre = it.comp.compare(0, 4, "ring") == 0;
        // the transient event marks over the stack (UI_HUD2_PRESETS.md 1.3: the chevrons and the hit flash draw on top)
        it.overlay = it.comp == "ring.hit" || it.comp == "ring.missiles";
        it.lane = it.comp == "context" ? hud2lint::Lane::Pill
                  : it.comp == "alert" ? hud2lint::Lane::Answer
                                       : hud2lint::Lane::None;
        if (d.kind == 'T' || d.kind == 'K')
        {
            it.textH = d.size;
            std::string t = text::Utf8(d.text);
            if (t.size() > 28)
                t = t.substr(0, 26) + "..";
            it.label = t;
        }
        else if (d.kind == 'G')
            it.label = glyphs::Name(static_cast<glyphs::Id>(d.glyph));
        else
            it.label = d.kind == 'P' ? "plate" : d.kind == 'A' ? "arc" : d.kind == 'I' ? "icon" : "shape";
        if (d.kind == 'T' && it.comp != "list")
            if (const ValueField* f = ValueOf(d.text, m))
            {
                it.value = f->id;
                // a declared second channel of a critical state (UI_HUD2_PRESETS.md 1.5 / 1.7: the FLARE mirror, the
                // hull flash) is not a second place
                const uimeta::hud2p::ValueId* vid = Declared(*f);
                for (int s = 0; vid && s < vid->secondCount; ++s)
                    if (it.comp == vid->second[s])
                        it.secondChannel = true;
            }
        li.items.push_back(std::move(it));
    }
    for (const ValueField& e : kValueFields)
        if (!Declared(e))
            li.na.push_back({std::string("value.id:") + e.id,
                             std::string("not declared with live '") + e.field +
                                 "' in UI_SPEC.json hud2_presets value_ids (core\\ui_meta.h): not matched"});
    // a text is plated when a drawn plate holds its box (informational: no item-3 rule reads it)
    for (hud2lint::Item& t : li.items)
        if (t.kind == hud2lint::Kind::Text)
            for (const hud2lint::Item& p : li.items)
                if (p.kind == hud2lint::Kind::Plate && p.alpha > 0 && t.x0 >= p.x0 && t.x1 <= p.x1 && t.y0 >= p.y0 &&
                    t.y1 <= p.y1)
                {
                    t.plated = true;
                    break;
                }

    // ---- zones, budgets, caps: the ACTIVE preset's data (hud_preset; ui_meta.h uimeta::hud2p from UI_SPEC.json
    // "hud2_presets"; UI_HUD2_PRESETS.md 6 step 4). Until step 5 a non-Standard preset draws as Standard (its header says
    // so): its findings then measure how far today's HUD is from that preset's rules, by design.
    const uimeta::hud2p::Preset& preset = hud2preset::Data(m.preset);
    // (the consumables' cross stacked on the loadout cuts the rail at its top: the owner's decision 3; the lower centre's
    // clear zone is every preset's, from the drawn ring's lowest edge + 0.01 H: decision 4, UI_HUD2.md 5.23)
    li.zones = hud2lint::ZonesOf(preset, L.W, L.H, L.tgtX0, L.tgtX1, m.kbm, L.consY1 > L.consY0 ? L.consX0 : 0,
                                 L.consY1 > L.consY0 ? L.consY0 : 0, L.clearY0);
    const hud2lint::Limits lim = hud2lint::LimitsOf(preset, m.state == State::Combat, m.kbm);
    li.budget = lim.budget, li.pillCap = lim.pills, li.pairCap = lim.pairs;

    // ---- the bound actions' reach: not in the HUD 2 state
    li.na.push_back({"action.reach",
                     "HUD 2 keeps no catalogue of the actions its prompts can show (a frame shows only its context's "
                     "prompts) and the wheel's entries are wheel.cpp's; needs step 1's controller-setup requirements"});

    // ---- the analog controls: the throttle lever (its readout = the ring's throttle arc); steer / aim read on the game's
    // own reticle (native, not HUD 2's draw)
    {
        glyphs::Id b = glyphs::COUNT;
        int slot = 0;
        const bool lever = presets::Find(c.table, presets::Action::Throttle, b, slot) || m.kbm;
        const float t = fabsf(m.throttle);
        // "while the lever rests between 1 and 99 %" (UI_HUD2_PRESETS.md 4, the ACE and K&M rows)
        hud2lint::Analog a;
        a.name = "throttle";
        a.inUse = lever && t > uimeta::hud2p::kLeverPartialLo && t < uimeta::hud2p::kLeverPartialHi;
        // (Standard's reading is the ring's throttle arc; Focus's the band's 3-segment tick, a segmented bar)
        for (const hud2lint::Item& it : li.items)
            if (it.comp == "ring.throttle" && (it.kind == hud2lint::Kind::Arc || it.kind == hud2lint::Kind::Shape) &&
                it.alpha > 0)
                a.readout = true;
        li.analogs.push_back(a);
        li.na.push_back({"analog.readout:steer/aim", "their reading is the game's reticle (native, not drawn by HUD 2)"});
    }

    // ---- latched toggles and their state words (UI_HUD2_PRESETS.md 4 holds_as_toggles row; 1.4 turret / rear view)
    {
        const bool toggles = uiopt::Get().holdsAsToggles;
        const struct
        {
            const char* name;
            bool latched;
            const wchar_t* word;
        } t[] = {{"afterburner", toggles && m.burner, L"BOOST"},
                 {"reverse", toggles && m.reverse, L"REV"},
                 {"engine kill", m.kill, L"KILL"},
                 {"turret view", m.turret, L"TURRET VIEW"},
                 {"rear view (wheel)", wheel::RearView(), L"REAR VIEW"}};
        for (const auto& e : t)
        {
            hud2lint::Toggle g;
            g.name = e.name, g.latched = e.latched, g.stateWord = e.latched && DrawnText(li.items, g_lintLast, e.word);
            li.toggles.push_back(g);
        }
        li.na.push_back({"toggle.state:rear view (held)",
                         "a rear view latched by holds_as_toggles on a button is the dispatcher's (controller.cpp), not "
                         "HUD 2 state: only the wheel's latch is checked"});
    }

    const hud2lint::Result r = hud2lint::Lint(li);
    std::ostringstream o;
    o << "hud2 lint " << static_cast<int>(L.W) << "x" << static_cast<int>(L.H) << " k=" << L.k
      << " preset=" << hud2preset::Label(m.preset) << " state=" << StateName(m.state) << " kbm=" << m.kbm
      << " items=" << li.items.size() << " frame_age_ms=" << (GetTickCount() - g_lintTick) << "\n";
    o << hud2lint::Format(r);
    return o.str();
}
} // namespace hud2
