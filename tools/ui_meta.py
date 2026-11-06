#!/usr/bin/env python
"""The UI metadata generator (docs\\UI_BUILD.md section 1 "UI metadata"): docs\\UI_SPEC.json -> core\\ui_meta.h, constexpr
tables of the spec's tabs / sections / rows (key, bus section, registry key, kind, label, options, min / max / step,
unit, words, tier, needs, parent, preset flag, button caption, help) plus the autopilot and controller preset tables and
the bindings presets / common situations (data for the HUD track), and `uimeta::hud2p` from the spec's `hud2_presets`
(the HUD 2 layout presets, modules\\hud\\docs\\UI_HUD2_PRESETS.md: shared numbers, zones, budget / prompt caps, value ids, each
element's numbers per context - read by the HUD 2 lint, modules\\hud\\src\\hud2_lint.h, and the layout core,
modules\\hud\\src\\hud2_layout.h; a spec without the block fails the generation). The options page (core\\ui\\options_page.*)
reads rows, defaults and values from the settings bus and uses this file for the layout and the words.

The alias table ALIASES maps a spec key to (bus section, registry key, value coding) where the registry's key or value
coding differs from the spec's: a picker's option i is written to the registry as `values[i]` when the alias gives a
list, else as its index; a slider's value is divided by `scale` on the way in (autopilot_cancel: the spec's 0..100 % over
the registry's 0..1). Every other spec key is its own registry key in the tab's default section (GENERAL -> Modules /
Controller, CONTROLLER -> Controller, AUTOPILOT -> Autopilot, SUBTITLES / MOVIES -> Subtitles).

Usage (the repository's Python: research\\tools\\ocr\\.venv\\Scripts\\python.exe -I):
    tools\\ui_meta.py            regenerate core\\ui_meta.h
    tools\\ui_meta.py --check    exit 1 when core\\ui_meta.h differs from a fresh generation (tools\\drift_check.ps1 "ui-meta")
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(ROOT, "docs", "UI_SPEC.json")
OUT = os.path.join(ROOT, "core", "ui_meta.h")

# tab id -> the bus section its rows live in unless an alias says otherwise
TAB_SECTION = {"general": "Controller", "controller": "Controller", "autopilot": "Autopilot",
               "subtitles": "Subtitles", "movies": "Subtitles"}

# spec key -> dict(section=, key=, values=[registry value per option], scale=, virtual=True)
ALIASES = {
    # GENERAL: the module switches live in FLvibed.ini [Modules] (the HOST track registers the section)
    "mod_subtitles": dict(section="Modules", key="subtitles"),
    "mod_controller": dict(section="Modules", key="controller"),
    "mod_hud": dict(section="Modules", key="hud"),
    "mod_autopilot": dict(section="Modules", key="autopilot"),
    "mod_speedrun": dict(section="Modules", key="speedrun"),
    "mod_display": dict(section="Modules", key="display"),
    "mod_npc": dict(section="Modules", key="npc"),
    "mod_movies": dict(section="Modules", key="movies"),
    # GENERAL buttons: no registry row (the page acts)
    "reset_all": dict(virtual=True),
    "about": dict(virtual=True),
    # CONTROLLER
    "aim_mode": dict(values=["1", "0"]),            # GIMBAL = 1, RETICLE = 0 (controller.cpp kOptions)
    "button_legend": dict(values=["0", "1"]),       # ON = 0, OFF = 1 (the registry row's coding: 0 on, 1 off)
    "edit_bindings": dict(virtual=True),            # the Bindings screen (phase D)
    # AUTOPILOT
    "preset": dict(virtual=True),                   # the preset table applied through the bus; CUSTOM when a row deviates
    "autopilot_cancel": dict(section="Controller", scale=100.0),  # [Controller] autopilot_cancel 0..1 <- 0..100 %
    "autopilot_key": dict(section="Controller"),
    "maneuver_button": dict(section="Controller"),
    "speedrun_timer": dict(section="Controller"),
    "livesplit": dict(section="Controller"),        # OFF = 0, LIVESPLIT SERVER (TCP) = 1
    "livesplit_port": dict(section="Controller"),
    "livesplit_gametime": dict(section="Controller"),
    # the game's [Pilot] blocks (DATA\MISSIONS\pilots_population.ini, verified 25 Oct 2026)
    "pilot": dict(values=["pilot_military_hard", "pilot_military_med", "pilot_pirate_ace", "pilot_company_med"]),
    # SUBTITLES: registry codings that differ from the option order
    "background_style": dict(values=["1", "0"]),    # HUD PANEL = 1, PLAIN = 0
    "space_position": dict(values=["2", "1", "0"]), # TOP PANEL = 2, TOP THIRD = 1, BOTTOM = 0
}

BUTTONS = ["A", "B", "X", "Y", "LB", "RB", "LT", "RT", "L3", "R3", "LS", "RS", "DU", "DD", "DL", "DR", "VIEW", "MENU"]


def cstr(s):
    """A C string literal; non-ASCII as UTF-8 \\x escapes, the literal split after each so a following hex digit is not
    swallowed (the source stays plain ASCII whatever MSVC's code page)."""
    if s is None:
        return "nullptr"
    out = '"'
    for ch in s:
        if ch == '"':
            out += '\\"'
        elif ch == "\\":
            out += "\\\\"
        elif ch == "\n":
            out += " "
        elif ord(ch) < 0x20 or ord(ch) > 0x7E:
            out += "".join("\\x%02X" % b for b in ch.encode("utf-8")) + '" "'
        else:
            out += ch
    out += '"'
    return out.replace('" ""', '"')


def num(v):
    """A float literal body (always with a decimal point: `90.0`, `0.05`), for an `f` suffix."""
    if v is None or isinstance(v, bool):
        return "1.0" if v is True else "0.0"
    s = ("%.6f" % float(v)).rstrip("0").rstrip(".")
    return s if "." in s else s + ".0"


def reg_default(row, alias):
    """The spec's `def` as the registry string the bus compares with (toggle 0/1, picker by coding, slider / scale)."""
    d = row.get("def")
    kind = row["kind"]
    if d is None:
        return None
    if kind == "toggle":
        return "1" if d else "0"
    if kind == "picker":
        opts = row.get("options", [])
        if d not in opts:
            raise SystemExit("%s: def %r not among its options" % (row["key"], d))
        i = opts.index(d)
        vals = alias.get("values")
        return vals[i] if vals else str(i)
    if kind == "slider":
        v = float(d) / alias.get("scale", 1.0)
        return ("%g" % v)
    if kind == "key":
        return str(d)
    return None


def ident(s):
    return "".join(c if c.isalnum() else "_" for c in s)


def gen(spec):
    L = []
    w = L.append
    w("// GENERATED by tools\\ui_meta.py from docs\\UI_SPEC.json (the decided UI; the mock-up is the living spec) - do not edit.")
    w("// The options page's layout metadata (docs\\UI_BUILD.md 1 \"UI metadata\"): tabs, sections and rows with their kind, words,")
    w("// range, tier, needs, parent, preset flag and help; a row's bus section and registry key (the alias table in the script")
    w("// maps the keys and value codings that differ); the autopilot / controller preset tables as registry values; the")
    w("// bindings presets and the shared situations (data for the pad HUD). tools\\drift_check.ps1 flags this file stale.")
    w("#pragma once")
    w('#include "common.h"')
    w("")
    w("namespace uimeta")
    w("{")
    w("enum Kind : char")
    w("{")
    w("    Toggle = 't', // a Bool row: checkbox + ENABLED / DISABLED")
    w("    Picker = 'p', // a word row: box with arrows; option i -> values[i] (or the index) in the registry")
    w("    Slider = 's', // a number row: the game's slider with a value caption; registry value = ui value / scale")
    w("    Key = 'k',    // a String row holding a key name; click / A = press a key")
    w("    Button = 'b'  // an action (no registry row)")
    w("};")
    w("enum Tier : char { Player = 'P', Advanced = 'A' };")
    w("enum Needs : char { Live = 'l', Load = 'd', Restart = 'r' };")
    w("")
    w("struct Row")
    w("{")
    w("    const char* key;     // the spec's key (the mock-up's id)")
    w("    const char* section; // the bus section (\"Controller\", \"Autopilot\", \"Subtitles\", \"Modules\")")
    w("    const char* regKey;  // the registry key; null = virtual (a preset row, a button): no registry row")
    w("    Kind kind;")
    w("    const char* label;")
    w("    const char* const* options; // Picker: the words; Slider: the step words (may be empty)")
    w("    int optionCount;")
    w("    const char* const* values;  // Picker: the registry value per option; null = the option's index")
    w("    float min, max, step;       // Slider (ui units)")
    w("    float scale;                // Slider: registry value = ui value / scale (1 = the same)")
    w("    const char* unit;           // Slider: appended to the caption (\"%\", \" s\", \" m\", a degree sign)")
    w("    const char* def;            // the spec's default as the registry string (null for buttons)")
    w("    Tier tier;")
    w("    Needs needs;")
    w("    const char* parent; // the spec key of the row that greys this one out when off; null = none")
    w("    bool preset;        // the tab's preset row (applies a preset table; CUSTOM when a member row deviates)")
    w("    const char* btn;    // Button: its caption")
    w("    const char* help;   // the rollover line")
    w("};")
    w("")
    w("struct Section")
    w("{")
    w("    const char* label; // \"\" = no header")
    w("    const Row* rows;")
    w("    int count;")
    w("};")
    w("")
    w("struct Tab")
    w("{")
    w("    const char* id;")
    w("    const char* label;")
    w("    const char* presetKey; // the preset row's spec key, or null")
    w("    const Section* sections;")
    w("    int count;")
    w("};")
    w("")
    w("// A preset's member as the bus takes it.")
    w("struct PresetValue")
    w("{")
    w("    const char* section;")
    w("    const char* key;   // registry key")
    w("    const char* value; // registry value")
    w("};")
    w("struct Preset")
    w("{")
    w("    const char* name; // the picker word")
    w("    const PresetValue* values;")
    w("    int count;")
    w("};")
    w("")
    w("// A pad button's actions in one situation (the Bindings view's words; tap / hold / double tap, \"none\" = nothing).")
    w("struct Bind")
    w("{")
    w("    const char* button; // A B X Y LB RB LT RT L3 R3 LS RS DU DD DL DR VIEW MENU")
    w("    const char* tap;")
    w("    const char* hold;")
    w("    const char* dbl;")
    w("};")
    w("struct BindSet")
    w("{")
    w("    const char* name; // a flight preset (FREELANCER / ACE / ASSISTED) or a shared situation (menus, rooms, ...)")
    w("    const Bind* binds;")
    w("    int count;")
    w("};")
    w("")

    # ---- rows
    tab_defs = []
    for tab in spec["tabs"]:
        tid = tab["id"]
        sec_defs = []
        for si, sec in enumerate(tab["sections"]):
            rows_name = "kRows_%s_%d" % (tid, si)
            w("// %s / %s" % (tab["label"], sec["label"] or "(no header)"))
            row_inits = []
            for row in sec["rows"]:
                key = row["key"]
                alias = ALIASES.get(key, {})
                kind = row["kind"]
                virtual = alias.get("virtual", False) or kind == "button"
                section = alias.get("section", TAB_SECTION[tid])
                regkey = None if virtual else alias.get("key", key)
                opts = row.get("options") if kind == "picker" else row.get("words", [])
                opts = opts or []
                vals = alias.get("values")
                if vals is not None and len(vals) != len(opts):
                    raise SystemExit("%s: alias values count %d != options %d" % (key, len(vals), len(opts)))
                oname = "kOpt_%s" % ident(key)
                vname = "kVal_%s" % ident(key)
                if opts:
                    w("inline constexpr const char* %s[] = {%s};" % (oname, ", ".join(cstr(o) for o in opts)))
                if vals:
                    w("inline constexpr const char* %s[] = {%s};" % (vname, ", ".join(cstr(v) for v in vals)))
                kind_c = {"toggle": "Toggle", "picker": "Picker", "slider": "Slider", "key": "Key", "button": "Button"}[kind]
                tier_c = "Advanced" if row.get("tier") == "advanced" else "Player"
                needs_c = {"live": "Live", "load": "Load", "restart": "Restart"}[row.get("needs", "live")]
                row_inits.append(
                    "    {%s, %s, %s, %s, %s, %s, %d, %s, %sf, %sf, %sf, %sf, %s, %s, %s, %s, %s, %s, %s, %s}," % (
                        cstr(key), cstr(section), cstr(regkey), kind_c, cstr(row["label"]),
                        oname if opts else "nullptr", len(opts), vname if vals else "nullptr",
                        num(row.get("min") if kind == "slider" else 0), num(row.get("max") if kind == "slider" else 0),
                        num(row.get("step") if kind == "slider" else 0), num(alias.get("scale", 1.0)),
                        cstr(row.get("unit", "") or ""), cstr(reg_default(row, alias)) if not virtual else "nullptr",
                        tier_c, needs_c, cstr(row.get("parent")), "true" if row.get("preset") else "false",
                        cstr(row.get("btn")), cstr(row.get("help", ""))))
            w("inline constexpr Row %s[] = {" % rows_name)
            L.extend(row_inits)
            w("};")
            sec_defs.append("    {%s, %s, %d}," % (cstr(sec["label"]), rows_name, len(sec["rows"])))
        w("inline constexpr Section kSections_%s[] = {" % tid)
        L.extend(sec_defs)
        w("};")
        w("")
        tab_defs.append("    {%s, %s, %s, kSections_%s, %d}," % (cstr(tid), cstr(tab["label"]), cstr(tab.get("presetKey")), tid, len(tab["sections"])))
    w("inline constexpr Tab kTabs[] = {")
    L.extend(tab_defs)
    w("};")
    w("inline constexpr int kTabCount = %d;" % len(spec["tabs"]))
    w("")

    # ---- presets: spec words -> registry values through the rows' coding
    rows_by_key = {}
    for tab in spec["tabs"]:
        for sec in tab["sections"]:
            for row in sec["rows"]:
                rows_by_key[row["key"]] = (tab["id"], row)

    def preset_value(key, v):
        if key not in rows_by_key:
            raise SystemExit("preset member %s is not a spec row" % key)
        tid, row = rows_by_key[key]
        alias = ALIASES.get(key, {})
        section = alias.get("section", TAB_SECTION[tid])
        regkey = alias.get("key", key)
        if row["kind"] == "toggle":
            val = "1" if v else "0"
        elif row["kind"] == "picker":
            opts = row["options"]
            if v not in opts:
                raise SystemExit("preset %s: %r not among %s" % (key, v, opts))
            vals = alias.get("values")
            val = vals[opts.index(v)] if vals else str(opts.index(v))
        else:
            val = "%g" % (float(v) / alias.get("scale", 1.0))
        return section, regkey, val

    for group in ("autopilot", "controller"):
        names = []
        for p in spec["presets"][group]:
            pname = "kPreset_%s_%s" % (group, ident(p["name"]))
            w("inline constexpr PresetValue %s[] = {" % pname)
            for k in sorted(p["values"]):
                s, rk, val = preset_value(k, p["values"][k])
                w("    {%s, %s, %s}," % (cstr(s), cstr(rk), cstr(val)))
            w("};")
            names.append("    {%s, %s, %d}," % (cstr(p["name"]), pname, len(p["values"])))
        w("inline constexpr Preset k%sPresets[] = {" % group.capitalize())
        L.extend(names)
        w("};")
        w("inline constexpr int k%sPresetCount = %d;" % (group.capitalize(), len(spec["presets"][group])))
        w("")

    # ---- bindings
    def bindset(name, table, var):
        w("inline constexpr Bind %s[] = {" % var)
        for b in BUTTONS:
            e = table.get(b, {})
            w("    {%s, %s, %s, %s}," % (cstr(b), cstr(e.get("tap", "none")), cstr(e.get("hold", "none")), cstr(e.get("dbl", "none"))))
        w("};")
        return "    {%s, %s, %d}," % (cstr(name), var, len(BUTTONS))

    sets = []
    for pname in ("FREELANCER", "ACE", "ASSISTED"):
        sets.append(bindset(pname, spec["bindings"]["presets"][pname]["flight"], "kBind_%s" % pname))
    w("inline constexpr BindSet kBindingPresets[] = {")
    L.extend(sets)
    w("};")
    w("inline constexpr int kBindingPresetCount = 3;")
    w("inline constexpr const char* kBindingDefault = %s;" % cstr(spec["bindings"]["preset"]))
    w("")
    sets = []
    for cname in sorted(spec["bindings"]["common"]):
        sets.append(bindset(cname, spec["bindings"]["common"][cname], "kBind_common_%s" % ident(cname)))
    w("inline constexpr BindSet kBindingCommon[] = {")
    L.extend(sets)
    w("};")
    w("inline constexpr int kBindingCommonCount = %d;" % len(spec["bindings"]["common"]))
    w("")
    w("// The bar button (docs\\UI_PLAN.md 2): the default key and whether it is on.")
    w("inline constexpr const char* kBarKey = %s;" % cstr(spec["bar"]["key"]))
    w("")
    w("// A row by its spec key (null if none).")
    w("inline const Row* FindRow(const char* key)")
    w("{")
    w("    for (const Tab& t : kTabs)")
    w("        for (int s = 0; s < t.count; ++s)")
    w("            for (int r = 0; r < t.sections[s].count; ++r)")
    w("                if (strcmp(t.sections[s].rows[r].key, key) == 0)")
    w("                    return &t.sections[s].rows[r];")
    w("    return nullptr;")
    w("}")
    w("")
    w("// A row by its bus section and registry key (null if none: an advanced / raw registry row).")
    w("inline const Row* FindRegistryRow(const char* section, const char* regKey)")
    w("{")
    w("    for (const Tab& t : kTabs)")
    w("        for (int s = 0; s < t.count; ++s)")
    w("            for (int r = 0; r < t.sections[s].count; ++r)")
    w("            {")
    w("                const Row& row = t.sections[s].rows[r];")
    w("                if (row.regKey && strcmp(row.section, section) == 0 && strcmp(row.regKey, regKey) == 0)")
    w("                    return &row;")
    w("            }")
    w("    return nullptr;")
    w("}")
    gen_hud2(spec, w)
    w("} // namespace uimeta")
    return "\n".join(L) + "\n"


# ---- HUD 2 layout presets (modules\hud\docs\UI_HUD2_PRESETS.md 6 step 1; UI_SPEC.json "hud2_presets")
HUD2_PRESET_IDS = ["standard", "focus", "speedrun", "guided"]  # hud_preset 0..3 (PRESETS 5)
HUD2_ANCHORS = {"centre_to_card": "CentreToCard", "rail": "Rail", "centre": "Centre", "centre_offset": "CentreOffset"}


def fnum(v, what):
    if v is None or isinstance(v, bool) or not isinstance(v, (int, float)):
        raise SystemExit("hud2_presets: %s is not a number (%r)" % (what, v))
    return num(v) + "f"


def strlist(w, name, items):
    """A static array of C strings (or nullptr when empty); returns (expr, count)."""
    if not items:
        return "nullptr", 0
    w("inline constexpr const char* %s[] = {%s};" % (name, ", ".join(cstr(s) for s in items)))
    return name, len(items)


def gen_hud2(spec, w):
    """uimeta::hud2p: the shared numbers the lint uses, each preset's complete zones ("lint": true), its boxed-budget
    caps and prompt caps, the value ids with their homes. Fails closed: a spec without hud2_presets (e.g. regenerated
    from an artifact ui/spec that missed tools\\uimock\\patches\\2026-10-31_hud2_presets.json) or a missing number stops
    the generation instead of writing a header without them."""
    hp = spec.get("hud2_presets")
    if not hp:
        raise SystemExit("UI_SPEC.json has no hud2_presets: apply tools\\uimock\\patches\\2026-10-31_hud2_presets.json "
                         "(specpatch.py) to the spec first")
    sh = hp["shared"]
    w("")
    w("// ---- HUD 2 layout presets (modules\\hud\\docs\\UI_HUD2_PRESETS.md, revision 2; UI_SPEC.json \"hud2_presets\": every number with")
    w("// its section there). Fractions of H at 16:9 from the top-left unless named _W (of W) or d (from the centre); budgets")
    w("// = fractions of the screen's area (plates only). The lint (modules\\hud\\src\\hud2_lint.h) reads these.")
    w("namespace hud2p")
    w("{")
    shared = [
        ("kTextFloor", sh["text"]["floor"], sh["text"]["src"]),
        ("kTextFloorPx", sh["text"]["floor_px"], sh["text"]["src"]),
        ("kCentreGap", sh["centre_gap"]["min"], sh["centre_gap"]["src"]),
        ("kCoreR", sh["core"]["r"], sh["core"]["src"]),
        ("kKeepX0W", sh["keep_out"]["x0_w"], sh["keep_out"]["src"]),
        ("kKeepX1W", sh["keep_out"]["x1_w"], sh["keep_out"]["src"]),
        ("kKeepY0", sh["keep_out"]["y0"], sh["keep_out"]["src"]),
        ("kKeepY1", sh["keep_out"]["y1"], sh["keep_out"]["src"]),
        ("kTitleSafe", sh["title_safe"]["frac"], sh["title_safe"]["src"]),
        ("kBudgetDefault", sh["budget_default"]["frac"], sh["budget_default"]["src"]),
        # the layout core's shared numbers (PRESETS 6 step 5, modules\hud\src\hud2_layout.h)
        ("kMarginSideW", sh["margins"]["side_w"], sh["margins"]["src"]),
        ("kMarginTop", sh["margins"]["top"], sh["margins"]["src"]),
        ("kMarginBottom", sh["margins"]["bottom"], sh["margins"]["src"]),
        ("kAlertPlateH", sh["alerts"]["plate_h"], sh["alerts"]["src"]),
        ("kAspectK4x3", sh["aspect"]["4:3"]["k"], sh["aspect"]["src"]),
        ("kAspectK16x10", sh["aspect"]["16:10"]["k"], sh["aspect"]["src"]),
    ]
    for name, v, src in shared:
        w("inline constexpr float %s = %s; // %s" % (name, fnum(v, name), src.replace("\n", " ")))
    w("// Components exempt from every zone (%s)" % sh["zone_exempt_src"])
    _, exn = strlist(w, "kZoneExempt", sh["zone_exempt"])
    if not exn:
        raise SystemExit("hud2_presets: shared.zone_exempt is empty")
    w("inline constexpr int kZoneExemptCount = %d;" % exn)
    w("")
    w("enum class Anchor : char")
    w("{")
    w("    CentreToCard = 'c', // x: cx +- min(halfMax, the card's left edge - cx - cardGap (cardGapNarrow below 16:9)); y fixed")
    w("    Rail = 'r',         // x: from the previous zone's right edge to the card's right edge; y fixed")
    w("    Centre = 'o',       // a circle of radius r about the screen centre")
    w("    CentreOffset = 'd', // a rectangle dx0..dx1, dy0..dy1 (in H) from the screen centre")
    w("};")
    w("struct Zone")
    w("{")
    w("    const char* name;")
    w("    Anchor anchor;")
    w("    bool circle;")
    w("    float x0, y0, x1, y1;           // H at 16:9 (CentreOffset: dx0, dy0, dx1, dy1)")
    w("    float r;                        // a circle")
    w("    float halfMax, cardGap, cardGapNarrow; // CentreToCard")
    w("    const char* const* exemptKbm;   // components exempt on keyboard and mouse only (beside kZoneExempt)")
    w("    int exemptKbmCount;")
    w("    const char* src;")
    w("};")
    w("// A boxed-budget cap: context (calm / combat), device (pad / kbm), case (no_target / target / steady / peak).")
    w("struct BudgetCap")
    w("{")
    w("    const char* context;")
    w("    const char* device;")
    w("    const char* when;")
    w("    float cap;")
    w("};")
    w("// A prompt cap per context: context-line pills and prompt pairs (answers apart; PRESETS 1.6).")
    w("struct PromptCap")
    w("{")
    w("    const char* context;")
    w("    int pills, pairs;")
    w("};")
    w("// One number of an element's geometry in a context (the spec's contexts.<ctx>.elements[]: every numeric field, as")
    w("// written there - fractions of H at 16:9 unless the key says _w): what the layout core (hud2_layout.h) reads by name.")
    w("struct ElemNum")
    w("{")
    w("    const char* context;")
    w("    const char* elem;")
    w("    const char* key;")
    w("    float v;")
    w("};")
    w("struct Preset")
    w("{")
    w("    int index; // hud_preset")
    w("    const char* id;")
    w("    const char* name;")
    w("    float reticleR, pickR; // the drawn ring / arcs radius and the RS-click pick radius (PRESETS 1.3, 3)")
    w("    const Zone* zones;")
    w("    int zoneCount;")
    w("    const BudgetCap* budgets;")
    w("    int budgetCount;")
    w("    const PromptCap* prompts;")
    w("    int promptCount;")
    w("    const char* src;")
    w("    const ElemNum* nums; // the elements' numbers per context (Num() below)")
    w("    int numCount;")
    w("};")
    w("")
    presets = hp["presets"]
    ids = [p["id"] for p in presets]
    if ids != HUD2_PRESET_IDS:
        raise SystemExit("hud2_presets: presets must be %s in order (got %s)" % (HUD2_PRESET_IDS, ids))
    pdefs = []
    for p in presets:
        pid = ident(p["id"])
        if p["index"] != HUD2_PRESET_IDS.index(p["id"]):
            raise SystemExit("hud2_presets: %s index %r" % (p["id"], p["index"]))
        # zones (complete ones only)
        zinits = []
        for z in p["zones"]:
            if not z.get("lint"):
                continue
            a = z["anchor"]
            if a not in HUD2_ANCHORS:
                raise SystemExit("hud2_presets: %s zone %s anchor %r" % (p["id"], z["id"], a))
            circle = z["shape"] == "circle"
            if circle:
                box = [0, 0, 0, 0]
            elif a == "centre_offset":
                box = [z["dx0"], z["dy0"], z["dx1"], z["dy1"]]
            else:
                box = [z["x0"], z["y0"], z["x1"], z["y1"]]
            what = "%s zone %s" % (p["id"], z["id"])
            exk, exkn = strlist(w, "kZoneExKbm_%s_%s" % (pid, ident(z["id"])), z.get("exempt_kbm", []))
            zinits.append("    {%s, Anchor::%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %d, %s}," % (
                cstr(z["id"]), HUD2_ANCHORS[a], "true" if circle else "false",
                fnum(box[0], what), fnum(box[1], what), fnum(box[2], what), fnum(box[3], what),
                fnum(z["r"], what) if circle else "0.0f",
                fnum(z.get("half_max", 0), what), fnum(z.get("card_gap", 0), what), fnum(z.get("card_gap_narrow", 0), what),
                exk, exkn, cstr(z["src"])))
        w("inline constexpr Zone kZones_%s[] = {" % pid)
        for zi in zinits:
            w(zi)
        w("};")
        # budget caps (every cap_* number given)
        binits = []
        for ctx in ("calm", "combat"):
            for dev in ("pad", "kbm"):
                b = p["budgets"].get(ctx, {}).get(dev, {})
                for when in ("no_target", "target", "steady", "peak"):
                    v = b.get("cap_" + when)
                    if v is not None:
                        binits.append("    {%s, %s, %s, %s}," % (cstr(ctx), cstr(dev), cstr(when),
                                                               fnum(v, "%s %s %s cap" % (p["id"], ctx, dev))))
        if binits:
            w("inline constexpr BudgetCap kBudgets_%s[] = {" % pid)
            for bi in binits:
                w(bi)
            w("};")
        pinits = []
        for ctx in ("calm", "combat"):
            c = p["prompts"][ctx]
            pinits.append("    {%s, %d, %d}," % (cstr(ctx), int(c["pills"]), int(c["pairs"])))
        w("inline constexpr PromptCap kPrompts_%s[] = {" % pid)
        for pi in pinits:
            w(pi)
        w("};")
        # the elements' numbers per context (the layout core reads them by context / element / key)
        ninits = []
        for ctx, c in p["contexts"].items():
            for e in c.get("elements", []):
                for key, v in e.items():
                    if isinstance(v, bool) or not isinstance(v, (int, float)):
                        continue
                    ninits.append("    {%s, %s, %s, %s}," % (cstr(ctx), cstr(e["id"]), cstr(key),
                                                          fnum(v, "%s %s %s %s" % (p["id"], ctx, e["id"], key))))
        # the reticle's and the cruise row's numbers (context "reticle", element "reticle" / "cruise_row"; a list of
        # numbers -> key_0, key_1, ...; Focus's split arcs and band read them: hud2_layout.h FocusNumbers)
        for elem in ("reticle", "cruise_row"):
            for key, v in p.get(elem, {}).items():
                vals = v if isinstance(v, list) else [v]
                if not vals or any(isinstance(x, bool) or not isinstance(x, (int, float)) for x in vals):
                    continue
                for i, x in enumerate(vals):
                    k = key if not isinstance(v, list) else "%s_%d" % (key, i)
                    ninits.append("    {%s, %s, %s, %s}," % (cstr("reticle"), cstr(elem), cstr(k),
                                                          fnum(x, "%s %s %s" % (p["id"], elem, k))))
        # the preset's settings bundle (PRESETS 5; context "bundle", element "existing" / "new"): its column of every
        # numeric key (null = the preset writes none) - the layout reads the values its bundle names (Focus: the calm
        # ring / panel opacities, the wake, the lingers, hud_reveal, hud_auto_peek) until step 4 writes the bundles
        bi = HUD2_PRESET_IDS.index(p["id"])
        for part in ("existing", "new"):
            for key, col in hp.get("bundles", {}).get(part, {}).items():
                x = col[bi] if isinstance(col, list) and bi < len(col) else None
                if x is None or isinstance(x, bool) or not isinstance(x, (int, float)):
                    continue
                ninits.append("    {%s, %s, %s, %s}," % (cstr("bundle"), cstr(part), cstr(key),
                                                      fnum(x, "%s bundle %s" % (p["id"], key))))
        if ninits:
            w("inline constexpr ElemNum kNums_%s[] = {" % pid)
            for ni in ninits:
                w(ni)
            w("};")
        ret = p["reticle"]
        rr = ret.get("ring_r", ret.get("arc_r"))
        pdefs.append("    {%d, %s, %s, %s, %s, kZones_%s, %d, %s, %d, kPrompts_%s, %d, %s, %s, %d}," % (
            p["index"], cstr(p["id"]), cstr(p["name"]), fnum(rr, p["id"] + " reticle r"),
            fnum(ret["pick_r"], p["id"] + " pick r"), pid, len(zinits),
            ("kBudgets_%s" % pid) if binits else "nullptr", len(binits), pid, len(pinits), cstr(p["src"]),
            ("kNums_%s" % pid) if ninits else "nullptr", len(ninits)))
    w("inline constexpr Preset kPresets[] = {")
    for pd in pdefs:
        w(pd)
    w("};")
    w("inline constexpr int kPresetCount = %d;" % len(presets))
    w("")
    w("// One id per HUD value (PRESETS 1.7: one value, one place): its home per preset (index = hud_preset; homeOff = while")
    w("// the target is off screen, null = the same), the declared second channels (components), the HUD 2 model field the")
    w("// lint matches drawn text against (live; null = not matched yet).")
    w("struct ValueId")
    w("{")
    w("    const char* id;")
    w("    const char* live;")
    w("    const char* home[4];")
    w("    const char* homeOff[4];")
    w("    const char* const* second;")
    w("    int secondCount;")
    w("    const char* src;")
    w("};")
    vinits = []
    seen = set()
    for v in hp["value_ids"]["ids"]:
        if v["id"] in seen:
            raise SystemExit("hud2_presets: value id %s twice" % v["id"])
        seen.add(v["id"])
        sec, secn = strlist(w, "kSecond_%s" % ident(v["id"]), v.get("second", []))
        home = [cstr(v["home"].get(i)) for i in HUD2_PRESET_IDS]
        off = [cstr(v.get("home_off", {}).get(i)) for i in HUD2_PRESET_IDS]
        vinits.append("    {%s, %s, {%s}, {%s}, %s, %d, %s}," % (cstr(v["id"]), cstr(v.get("live")), ", ".join(home),
                                                             ", ".join(off), sec, secn, cstr(v["src"])))
    w("inline constexpr ValueId kValueIds[] = {")
    for vi in vinits:
        w(vi)
    w("};")
    w("inline constexpr int kValueIdCount = %d;" % len(vinits))
    w("")
    gen_hud2_setups(hp, w)
    w("")
    w("inline const ValueId* FindValueId(const char* id)")
    w("{")
    w("    for (const ValueId& v : kValueIds)")
    w("        if (strcmp(v.id, id) == 0)")
    w("            return &v;")
    w("    return nullptr;")
    w("}")
    w("inline const PromptCap* Prompts(const Preset& p, const char* context)")
    w("{")
    w("    for (int i = 0; i < p.promptCount; ++i)")
    w("        if (strcmp(p.prompts[i].context, context) == 0)")
    w("            return &p.prompts[i];")
    w("    return nullptr;")
    w("}")
    w("// An element's number in a context (e.g. Num(p, \"combat\", \"card\", \"h_off_screen\", ...)); `fallback` when the spec")
    w("// gives none (the caller's documented default: fail closed to a known layout).")
    w("inline float Num(const Preset& p, const char* context, const char* elem, const char* key, float fallback)")
    w("{")
    w("    for (int i = 0; i < p.numCount; ++i)")
    w("        if (strcmp(p.nums[i].context, context) == 0 && strcmp(p.nums[i].elem, elem) == 0 &&")
    w("            strcmp(p.nums[i].key, key) == 0)")
    w("            return p.nums[i].v;")
    w("    return fallback;")
    w("}")
    w("inline bool HasNum(const Preset& p, const char* context, const char* elem, const char* key)")
    w("{")
    w("    return Num(p, context, elem, key, -1e30f) != -1e30f;")
    w("}")
    w("} // namespace hud2p")


HUD2_SETUP_COLUMNS = ["FREELANCER / CUSTOM", "ACE", "ASSISTED", "K&M"]  # PRESETS 4 compatibility table's columns
HUD2_FITS = {"fine": "Fine", "adapts": "Adapts", "discouraged": "Discouraged"}
# pad_preset 0..3 (core\ui\presets.h Preset) -> the spec's "suggested" key
HUD2_PAD_PRESETS = ["FREELANCER", "ACE", "ASSISTED", "CUSTOM"]


def gen_hud2_setups(hp, w):
    """The HUD preset x controller setup table (PRESETS 4; controller_setups.compat / suggested / lever_partial): each
    cell's fit word, its "(suggested)" mark and its note, the suggested HUD preset per controller setup. Fails closed on
    a missing cell or an unknown fit word (the HUD's adaptation read, hud2_preset.h, takes these as they are)."""
    cs = hp.get("controller_setups") or {}
    compat, sugg, lever = cs.get("compat"), cs.get("suggested"), cs.get("lever_partial")
    if not compat or not sugg or not lever or len(lever) != 2:
        raise SystemExit("hud2_presets: controller_setups needs compat, suggested and lever_partial [lo, hi]")
    src = cs.get("src", "PRESETS 4")
    w("// The HUD preset x controller setup table (%s; nothing is blocked): fit, the suggested pairing, the" % src)
    w("// note; [hud_preset][column]. Columns = the table's controller setups (CUSTOM = FREELANCER until the Bindings editor).")
    w("enum class Fit : char")
    w("{")
    w("    Fine = 'f',")
    w("    Adapts = 'a',      // the preset adds what the setup needs (unplated free text: no boxed budget change)")
    w("    Discouraged = 'd', // offered another preset, never blocked")
    w("};")
    w("struct Compat")
    w("{")
    w("    Fit fit;")
    w("    bool suggested;")
    w("    const char* note; // the words after the fit (\"\" none)")
    w("};")
    w("enum Column : int")
    w("{")
    w("    kColFreelancer = 0, // FREELANCER / CUSTOM")
    w("    kColAce = 1,")
    w("    kColAssisted = 2,")
    w("    kColKbm = 3,")
    w("};")
    _, ncol = strlist(w, "kColumnNames", HUD2_SETUP_COLUMNS)
    w("inline constexpr int kColumnCount = %d;" % ncol)
    w("inline constexpr Compat kCompat[%d][%d] = {" % (len(HUD2_PRESET_IDS), ncol))
    for pid in HUD2_PRESET_IDS:
        row = compat.get(pid)
        if not row:
            raise SystemExit("hud2_presets: controller_setups.compat has no %s row" % pid)
        cells = []
        for col in HUD2_SETUP_COLUMNS:
            t = row.get(col)
            if not t:
                raise SystemExit("hud2_presets: compat %s / %s missing" % (pid, col))
            word = t.split(":")[0].split("(")[0].strip()
            if word not in HUD2_FITS:
                raise SystemExit("hud2_presets: compat %s / %s: unknown fit %r" % (pid, col, word))
            note = t.split(":", 1)[1].replace("(suggested)", "").strip() if ":" in t else ""
            cells.append("{Fit::%s, %s, %s}" % (HUD2_FITS[word], "true" if "(suggested)" in t else "false", cstr(note)))
        w("    {%s}, // %s" % (", ".join(cells), pid))
    w("};")
    idx = []
    for k in HUD2_PAD_PRESETS + ["K&M"]:
        v = sugg.get(k)
        if v not in HUD2_PRESET_IDS:
            raise SystemExit("hud2_presets: controller_setups.suggested %s = %r" % (k, v))
        idx.append(str(HUD2_PRESET_IDS.index(v)))
    w("// The suggested HUD preset (hud_preset) per pad_preset 0..3 (FREELANCER, ACE, ASSISTED, CUSTOM), and on K&M")
    w("inline constexpr int kSuggestedPad[4] = {%s};" % ", ".join(idx[:4]))
    w("inline constexpr int kSuggestedKbm = %s;" % idx[4])
    w("// \"while the lever rests between 1 and 99 %%\" (%s)" % src)
    w("inline constexpr float kLeverPartialLo = %s;" % fnum(lever[0], "lever_partial lo"))
    w("inline constexpr float kLeverPartialHi = %s;" % fnum(lever[1], "lever_partial hi"))


def main():
    with open(SPEC, encoding="utf-8") as f:
        spec = json.load(f)
    text = gen(spec)
    if "--check" in sys.argv:
        old = open(OUT, encoding="utf-8").read().replace("\r\n", "\n") if os.path.exists(OUT) else ""
        if old == text:
            print("ui meta current (%d tabs)" % len(spec["tabs"]))
            return 0
        print("ui meta STALE: regenerate with tools\\ui_meta.py")
        return 1
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    rows = sum(len(s["rows"]) for t in spec["tabs"] for s in t["sections"])
    print("wrote %s (%d tabs, %d rows)" % (OUT, len(spec["tabs"]), rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
