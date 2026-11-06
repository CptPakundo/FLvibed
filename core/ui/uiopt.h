// The controller UI's settings (docs\UI_SPEC.json, CONTROLLER tab) as the HUD track's modules read them (device,
// glyphs, presets, padhud): one struct, field names = the registry's Options fields, defaults = the spec's `def`. The
// registry rows are controller.cpp's kOptions (the PAGE track); controller.cpp copies the Options fields in here every
// frame, so the modules never reach into its file-local Options.
#pragma once
#include "common.h"
#include "settings.h"

namespace uiopt
{
struct UiOptions
{
    int inputMode = 0;              // input_mode: 0 automatic, 1 controller, 2 keyboard and mouse
    int glyphStyle = 0;             // glyph_style: 0 automatic (HID vendor), 1 Xbox, 2 PlayStation, 3 Nintendo, 4 Steam
    int glyphLook = 0;              // glyph_look: 0 automatic, 1 coloured everywhere, 2 HUD 2 glyphs everywhere (glyphs::LookFor);
                                    // its registry row points here directly (controller.cpp kOptions), no copy
    int confirmButton = 0;          // confirm_button: 0 bottom (A / cross / B), 1 right (Nintendo A)
    bool pauseOnDisconnect = true;  // pause_on_disconnect
    int padPreset = 0;              // pad_preset: 0 FREELANCER, 1 ACE, 2 ASSISTED, 3 CUSTOM (= FREELANCER until phase D)
    int weaponGroups = 1;           // weapon_groups: 0 simple, 1 cycle on Y hold, 2 two groups on the triggers
    bool swapY = false;             // y_swap
    bool holdsAsToggles = false;    // holds_as_toggles
    int rumble = 100;               // rumble: 0..100 %
    int padCursor = 1;              // pad_cursor: 0 off, 1 rooms and maps, 2 everywhere (phase D)
    int cursorMagnet = 60;          // cursor_magnet % (phase D)
    int promptBar = 0;              // prompt_bar: 0 contextual, 1 always, 2 off (phase D)
    int worldPrompts = 0;           // world_prompts: 0 contextual, 1 off
    bool apHints = true;            // ap_hints: show the autopilot's prompts to players who never used it (on since 0.2.0)
    int textSize = 100;             // text_size: 80..150 %
    bool hudGlyphs = true;          // hud_glyphs
    int buttonLegend = 0;           // button_legend: 0 on, 1 off
    bool apStatusLine = true;       // ap_status_line
    // HUD 2 (modules\hud\docs\UI_HUD2.md 5.3)
    int hudStyle = 2;               // hud_style: 0 CLASSIC, 1 HUD 2 on the pad, 2 HUD 2 always (default since 0.2.0)
    int hudFade = 1;                // hud_fade: 0 full, 1 contextual (0.2.1; core\game\hudwake.h)
    int hudOpacity = 100;           // hud_opacity %
    int hudScale = 100;             // hud_scale %
    int hudNumbers = 0;             // hud_numbers: 0 when low, 1 always, 2 never
    bool hudThreats = true;         // hud_threats
    int hudContext = 0;             // hud_context: 0 contextual, 1 off
    int hudPrompts = 1;             // hud_prompts: 0 always shown, 1 shown on a change / an action (0.2.1)
    bool hudAlerts = true;          // hud_alerts
    bool hudInfo = true;            // hud_info
    bool hudWireframes = true;      // hud_wireframes
    int hudTransition = 120;        // hud_transition ms
    int hudPreset = 0;              // hud_preset: 0 Standard, 1 Focus, 2 Speedrun, 3 Guided (UI_HUD2_PRESETS.md 6 step 4;
                                    // only Standard's layout is built: hud2_preset.h)
};
UiOptions& Get();

// Empty: the registry rows live in controller.cpp kOptions (kept for the interim fallback's call sites).
const settings::Def* Defs(size_t& count);
} // namespace uiopt
