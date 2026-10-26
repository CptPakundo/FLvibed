// FLvibed - the settings bus: every module's settings registry (core\settings.h kDefs) reachable from one place, so
// the options page (controller logic) can list, read, change, reset and save the settings of EVERY module - the
// subtitles, the movies, the controller, the autopilot and the module switches - without a second copy of any value.
// One instance lives in FLvibed.dll (core\settings_bus.cpp, compiled plain with core\flvibed.cpp); the module parts
// reach it through their HostApi (`GetSettingsBus`, controller host API v18 / subtitles host API v6). Each logic part
// registers its sections at Init and unregisters at Shutdown (hot reload: the pointers die with the logic DLL).
// A section's defaults are the registry's values at registration (the struct initialisers: register BEFORE the ini
// load) unless the module passes them. Changes go through `Set`, which applies the value with settings::Apply and
// tells the owner (`applied`) so it can react (rebuild fonts, re-place the HUD, re-arm a hook). UI_PLAN.md 3.4.
#pragma once
#include "common.h"
#include "settings.h"

// The bus is one struct of plain function pointers (no C++ linkage across the DLL boundary / the module namespaces).
struct SettingsBusSection
{
    const char* module;        // "subtitles", "controller", "autopilot", "core" (who owns it; logging)
    const char* section;       // the ini section AND the UI metadata's section name: "Subtitles", "Controller",
                               // "Autopilot", "Modules", "UI"
    const settings::Def* defs; // the registry rows (the owner keeps them alive until Unregister)
    size_t count;
    void* ctx;
    // After `Set` applied a value (and after `Reset`), with the row changed; may be null.
    void (*applied)(void* ctx, const settings::Def* def);
    // Write the section to its ini (the owner's writer); may be null (the bus then does nothing on Save).
    void (*save)(void* ctx);
    // Defaults as strings, one per def (the owner's, when its initialisers are not the defaults); null = snapshot.
    const char* const* defaults;
};

struct SettingsBus
{
    u32 version; // 1
    // Registers a section; returns its id (> 0) or 0 when the bus is full / the section name is taken.
    int (*Register)(const SettingsBusSection* section);
    void (*Unregister)(int id);
    int (*Count)();
    // The i-th registered section (0 <= i < Count), or null.
    const SettingsBusSection* (*Section)(int i);
    // A row by section name and key, or null.
    const settings::Def* (*Find)(const char* section, const char* key);
    // The default of a row (as registered / snapshotted), or "" when unknown. `out` holds at least 96 chars.
    bool (*Default)(const char* section, const char* key, char* out, int outLen);
    // Applies a value (settings::Apply) and tells the owner. False when the row is unknown or the value does not parse.
    bool (*Set)(const char* section, const char* key, const char* value);
    // The current value as text (settings::Value); false when unknown.
    bool (*Get)(const char* section, const char* key, char* out, int outLen);
    // Back to the default (Set with the default).
    bool (*Reset)(const char* section, const char* key);
    // Writes the section's ini through the owner's writer; false when unknown or the owner has no writer.
    bool (*Save)(const char* section);
    // A change counter: grows with every Set / Reset on any section (the page's dirty tracking, the HUD's caches).
    u32 (*Generation)();
};
