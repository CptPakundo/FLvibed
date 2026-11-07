// FLvibed - the HUD's settings rows and their values. Core holds them because two tables list them: the controller's registry at the places they always had
// in `[Controller]` (so FLController.ini, `config` and docs\SETTINGS.md keep their order), and the HUD's own when the controller is off
// (core\logic\alone.cpp). One definition each. HUD 2 reads the values through uiopt, the cues and the text through `g`.
#pragma once
#include "common.h"
#include "settings.h"

namespace hudrows
{
struct ShieldCue
{
    bool on = true;
    float intensity = 1.0f; // 0..2: alpha and size scale
    bool hull = true;       // hull hits in the ship's hull hit look + the damage state
    int pattern = 2;        // hull damage edge pattern: 0 none, 1 armour plates, 2 hull seams
};
struct ReticleCue
{
    bool on = true;
};
struct Values
{
    ShieldCue shield;
    ReticleCue reticle;
    int gameTextSize = 0;   // the game's own text, percent (100..150; gametext.h), 0 automatic: it follows hud_size
    bool windowText = true; // the game's windows' text by role, HUD 2 only (windowtext.h)
    // HUD 2 (modules\hud\docs\UI_HUD2.md 5.3; the codings: the rows' help)
    int nnPause = 1, basePages = 1, hudStyle = 2, hudFade = 1, hudOpacity = 100, hudScale = 100, hudNumbers = 0, hudContext = 0, hudPrompts = 1;
    bool hudThreats = true, hudAlerts = true, hudInfo = true, hudWireframes = true, groupCue = true;
    int hudTransition = 120;
    int hudPreset = 0; // hud_preset: HUD 2's layout preset (UI_HUD2_PRESETS.md 6 step 4)
    int uiStyle = 1;   // ui_style: the non-flight windows' look, 0 CLASSIC, 1 HUD 2 (uiskin.*, uistrip.*); only with HUD 2 on
    int skinMotion = 0;     // skin_motion: the skin's own motion, 0 full, 1 fades, 2 none
    std::string skinNative; // skin_native: family labels that stay the game's (hidden; troubleshooting and reports)
};
inline Values g;
} // namespace hudrows

#define FLV_HUD_ROWS_CUES \
    {"shield_cue", settings::Bool, &hudrows::g.shield.on, 0, 1, \
     "screen-edge cue made of your shield's own hit effect when it is hit (tinted by the weapon), a flash when it " \
     "fails; HUD 2's shield meter flashes red when low, and its danger alerts glow and pulse"}, \
    {"shield_cue_intensity", settings::Float, &hudrows::g.shield.intensity, 0, 2, \
     "shield_cue strength (alpha and size)"}, \
    {"hull_cue", settings::Bool, &hudrows::g.shield.hull, 0, 1, \
     "with shield_cue: hull hits show your ship's own hull hit effect at the screen edge, and while the hull is " \
     "damaged the edges carry your ship's damage effects (sparks, fire, smoke) as the game lights them; HUD 2's hull " \
     "meter flashes bright red when low"}, \
    {"hull_cue_pattern", settings::Int, &hudrows::g.shield.pattern, 0, 2, \
     "pattern of the hull damage edge, so it never reads as a shield hit: 0 none, 1 armour plates, 2 hexagons " \
     "(default)"}, \
    {"reticle_cue", settings::Bool, &hudrows::g.reticle.on, 0, 1, \
     "a ring pulse at the crosshair: autopilot on (cyan, outward), handed back (amber, inward), nanobots (green) / " \
     "batteries (blue) used (double ring)"},

#define FLV_HUD_ROWS_TEXT \
    {"game_text_size", settings::Int, &hudrows::g.gameTextSize, 0, 150, \
     "size of the game's own text in every window: 0 automatic (follows hud_size), else 100 to 150 percent; takes " \
     "effect at the main menu"}, \
    {"window_text", settings::Bool, &hudrows::g.windowText, 0, 1, \
     "HUD 2: the text in the game's windows takes the room it has, the prices most, labels less, headings and buttons not at all"},

// HUD 2's layout presets (modules\hud\docs\UI_HUD2_PRESETS.md 6 step 4, the plumbing): not in UI_SPEC.json's rows yet, so the
// options page lists it only with show_advanced (raw, CONTROLLER tab) until the presets are released (the owner's
// frames per preset first: 6 step 8). 0.2.0 caps it at 1: Speedrun (2) and Guided (3) are not built and drew as
// Standard; raise the cap as each preset's layout lands (hud2_preset.h keeps their ids)
#define FLV_HUD_ROWS_HUD \
    {"hud_style", settings::Int, &hudrows::g.hudStyle, 0, 2, \
     "the HUD: 0 classic (the game's HUD and screens; on the pad with the glyph badges and the legend), 1 HUD 2 on the " \
     "pad (keyboard and mouse keep the game's HUD), 2 HUD 2 always (default); HUD 2 includes its look over the menus " \
     "and rooms (ui_style)"}, \
    {"nn_pause", settings::Int, &hudrows::g.nnPause, 0, 1, \
     "the NeuroNet windows in single player, in space: 1 the world stops while one is open (default; the autopilot flying keeps it going), 0 the game goes on behind them"}, \
    {"base_pages", settings::Int, &hudrows::g.basePages, 0, 1, \
     "the dealers, the ship dealer, the job board and the news: 1 HUD 2's pages (default), 0 the game's windows with HUD 2's skin"}, \
    {"hud_fade", settings::Int, &hudrows::g.hudFade, 0, 1, \
     "HUD 2 in flight: 0 full (always shown), 1 contextual (fades out in calm flying; each part comes back for 4 s when " \
     "it matters: steering, boost, a target, firing, a maneuver, a new objective; alerts, low shield / hull, missiles " \
     "and fights always show)"}, \
    {"hud_opacity", settings::Int, &hudrows::g.hudOpacity, 40, 100, "HUD 2's overall opacity in percent"}, \
    {"hud_scale", settings::Int, &hudrows::g.hudScale, 80, 130, "HUD 2's size in percent"}, \
    {"hud_numbers", settings::Int, &hudrows::g.hudNumbers, 0, 2, \
     "the numbers at the ring's shield and hull arcs: 0 only when low, 1 always, 2 never"}, \
    {"hud_threats", settings::Bool, &hudrows::g.hudThreats, 0, 1, \
     "HUD 2 lists the next hostiles under the target card during a fight"}, \
    {"hud_context", settings::Int, &hudrows::g.hudContext, 0, 1, \
     "the buttons for what you can do now, under your ship: 0 contextual, 1 off"}, \
    {"hud_prompts", settings::Int, &hudrows::g.hudPrompts, 0, 1, \
     "when the context prompts show in flight: 0 always, 1 when they change or right after you act (dock, cruise, " \
     "a target, a maneuver), then they fade out after 4 seconds"}, \
    {"group_cue", settings::Bool, &hudrows::g.groupCue, 0, 1, \
     "the weapon group cue above the ring: your groups, M and T, filled when the target is in the centre and in reach"}, \
    {"hud_alerts", settings::Bool, &hudrows::g.hudAlerts, 0, 1, \
     "short warning cards (shield failing, missile incoming, cargo hold full, loot the tractor refused and why) with the buttons that answer them"}, \
    {"hud_info", settings::Bool, &hudrows::g.hudInfo, 0, 1, \
     "extra information when it matters: docking queue, jump countdown, mission timers, formation, tractor"}, \
    {"hud_wireframes", settings::Bool, &hudrows::g.hudWireframes, 0, 1, \
     "the game's wireframe views in the target card and the ship panel where they can be kept (else flat icons)"}, \
    {"hud_transition", settings::Int, &hudrows::g.hudTransition, 0, 400, \
     "advanced: how long HUD 2 takes to fade in when you switch to the controller, in milliseconds"}, \
    {"hud_preset", settings::Int, &hudrows::g.hudPreset, 0, 1, \
     "experimental: HUD 2's layout preset - 0 Standard (default: today's HUD 2), 1 Focus (a calmer HUD that shows less " \
     "until it matters); Speedrun and Guided are planned"},

// HUD 2 over the menus (formerly Extended HUD 2, modules\hud\docs\UI_HUD2_EXTENDED.md 9-10; folded into HUD 2 in 0.2.0): 1 HUD 2
// (default), 0 CLASSIC; applies only while hud_style is not 0
#define FLV_HUD_ROWS_STYLE \
    {"ui_style", settings::Int, &hudrows::g.uiStyle, 0, 1, \
     "The look of the menus, rooms and dialogs while HUD 2 is on: HUD 2 = its skin over the same screens (default; the " \
     "game's controls stay as they are); Classic = the game's own"}, \
    {"skin_motion", settings::Int, &hudrows::g.skinMotion, 0, 2, \
     "HUD 2's motion, in flight and in the menus and dialogs: 0 full, 1 fades (no breathing or travel, a change fades in place), 2 none (a change cuts)"}, \
    {"skin_native", settings::String, &hudrows::g.skinNative, 0, 0, \
     "advanced, for troubleshooting: a comma list of window names that keep the game's look (pause, navmap, cursor ...); empty by default"},

namespace hudrows
{
inline const settings::Def kRows[] = {FLV_HUD_ROWS_CUES FLV_HUD_ROWS_TEXT FLV_HUD_ROWS_HUD FLV_HUD_ROWS_STYLE};
inline constexpr size_t kRowCount = sizeof(kRows) / sizeof(kRows[0]);
} // namespace hudrows
