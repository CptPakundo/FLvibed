#!/usr/bin/env python
"""The UI metadata generator (docs\\UI_BUILD.md section 1 "UI metadata"): docs\\UI_SPEC.json -> core\\ui_meta.h, constexpr
tables of the spec's tabs / sections / rows (key, bus section, registry key, kind, label, options, min / max / step,
unit, words, tier, needs, parent, preset flag, button caption, help) plus the autopilot and controller preset tables and
the bindings presets / common situations (data for the HUD track). The options page (controller\\src\\impl\\options_page.*)
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
                    w("static constexpr const char* %s[] = {%s};" % (oname, ", ".join(cstr(o) for o in opts)))
                if vals:
                    w("static constexpr const char* %s[] = {%s};" % (vname, ", ".join(cstr(v) for v in vals)))
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
            w("static constexpr Row %s[] = {" % rows_name)
            L.extend(row_inits)
            w("};")
            sec_defs.append("    {%s, %s, %d}," % (cstr(sec["label"]), rows_name, len(sec["rows"])))
        w("static constexpr Section kSections_%s[] = {" % tid)
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
            w("static constexpr PresetValue %s[] = {" % pname)
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
        w("static constexpr Bind %s[] = {" % var)
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
    w("} // namespace uimeta")
    return "\n".join(L) + "\n"


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
