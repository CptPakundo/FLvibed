// FLvibed - the rows of `[Controller]` that act with the Controller module off. Two tables list them: the controller's registry at the places they always had, so
// FLController.ini, `config` and docs\SETTINGS.md keep their order, and core's when the controller is off (core\logic\alone.cpp), each row with the module that uses
// it. One definition each: a macro with the value's place. `Values` is what the two tables keep apart; the rows of core's own values name them directly.
#pragma once
#include "common.h"
#include "game/coreframe.h"
#include "game/modslots.h"
#include "settings.h"

#include <string>

namespace sharedrows
{
struct Values
{
    bool maneuverWatchdog = true; // a formation / goto / dock stalled far from its goal is issued again (core\game\maneuver_watch.h)
    // Full deflection = this offset in Freelancer's mouse-flight units (1 = cursor at the screen
    // edge). Measured: turn rate = offset * 1.3 * max rate (steering_torque / angular_drag), so
    // the ship reaches its max turn rate at ~0.77 and any further cursor offset is wasted.
    float steerScale = 0.77f;
    bool invertY = false;          // stick up = nose down
    float aimCone = 90.0f;         // gimbal mode: auto-aim cone half-angle (degrees); 90 = whole screen
    float autopilotCancel = 0.35f; // stick deflection that hands the ship back (0 = never)
    int borderless = 1;            // borderless: 0 off, 1 auto (only when nothing else provides it), 2 always; the host reads it at the start (winmode.h)
    bool apHints = true;           // the autopilot's prompts / status shown before it was ever used (on since 0.2.0)
    bool showAdvanced = false;     // the options page lists every registry row, raw
    bool speedrunTimer = true;     // speedrun mode: the timer / split on the HUD
    int livesplit = 1;             // 0 off, 1 LiveSplit Server (TCP)
    int livesplitPort = 16834;
    bool livesplitGametime = true; // pause LiveSplit's game time during loads
    std::string autopilotKey = "K"; // the keyboard key that toggles the autopilot (UI_PLAN.md 2: K is unbound)
    bool maneuverButton = true;    // the fifth maneuver-bar button
};
} // namespace sharedrows

#define FLV_ROW_STEER_SCALE(v) \
    {"steer_scale", settings::Float, &(v).steerScale, 0.1f, 1, \
     "full stick = this mouse-flight offset; 0.77 = the ship's maximum turn rate"},

#define FLV_ROW_AIM_CONE(v) \
    {"aim_cone", settings::Float, &(v).aimCone, 0, 90, \
     "gimbal: lock-on cone half-angle in degrees (90 = whole screen)"},

#define FLV_ROW_INVERT_Y(v) \
    {"invert_y", settings::Bool, &(v).invertY, 0, 1, "1 = stick up turns the nose down"},

#define FLV_ROW_MANEUVER_WATCHDOG(v) \
    {"maneuver_watchdog", settings::Bool, &(v).maneuverWatchdog, 0, 1, \
     "1 = if your ship stalls during a formation, goto or dock (nearly stopped for 3 s, its goal more than 150 m away " \
     "and not getting closer, you not steering), FLvibed leaves it and issues it again (up to 3 times, 10 s apart) and " \
     "logs what happened"},

#define FLV_ROW_AUTOPILOT_CANCEL(v) \
    {"autopilot_cancel", settings::Float, &(v).autopilotCancel, 0, 1, \
     "Advanced Autopilot (the pad hold, the key or the bar button starts it): stick deflection that hands the ship " \
     "back to you (0 = never). Its own settings (pilot, engage rules, weapons, personality fields p.<name>) are in " \
     "the [Autopilot] section"},

#define FLV_ROW_AP_HINTS(v) \
    {"ap_hints", settings::Bool, &(v).apHints, 0, 1, \
     "show the autopilot's prompts and status before it was ever used; 0 = nothing about it shows until you switch " \
     "it on once"},

#define FLV_ROW_AUTOPILOT_KEY(v) \
    {"autopilot_key", settings::String, &(v).autopilotKey, 0, 0, \
     "the keyboard key that toggles the autopilot (a key name: K, F10, Insert); a key the game binds is refused"},

#define FLV_ROW_MANEUVER_BUTTON(v) \
    {"maneuver_button", settings::Bool, &(v).maneuverButton, 0, 1, \
     "the fifth button on the maneuver bar (keyboard and mouse HUD) toggles the autopilot"},

#define FLV_ROW_BORDERLESS(v) \
    {"borderless", settings::Int, &(v).borderless, 0, 2, \
     "borderless windowed, read at the game's start: 0 off (the game's own window mode), 1 (default) only when nothing " \
     "else provides it (not with HDE: its exe and its Direct3D wrapper do), 2 always; while FLvibed provides it the " \
     "game runs as a window without frame covering its monitor at the monitor's resolution (never exclusive " \
     "fullscreen; sharp at high DPI scaling only when the exe is DPI-aware), and FLUF's Alt+Enter toggles it with the " \
     "bordered window"},

#define FLV_ROW_SHOW_ADVANCED(v) \
    {"show_advanced", settings::Bool, &(v).showAdvanced, 0, 1, \
     "the options page adds the developer and tuning keys to every tab, as they are named in the ini files"},

#define FLV_ROW_SPEEDRUN_TIMER(v) \
    {"speedrun_timer", settings::Bool, &(v).speedrunTimer, 0, 1, \
     "speedrun mode: the run time, the current split and the delta against your best on the HUD"},

#define FLV_ROW_LIVESPLIT(v) \
    {"livesplit", settings::Int, &(v).livesplit, 0, 1, \
     "send the timer and splits to LiveSplit on this computer: 0 off, 1 LiveSplit Server (TCP; Control > Start TCP " \
     "Server in LiveSplit 1.8.29 or newer)"},

#define FLV_ROW_LIVESPLIT_PORT(v) \
    {"livesplit_port", settings::Int, &(v).livesplitPort, 1024, 65535, "LiveSplit Server's port (default 16834)"},

#define FLV_ROW_LIVESPLIT_GAMETIME(v) \
    {"livesplit_gametime", settings::Bool, &(v).livesplitGametime, 0, 1, \
     "pause LiveSplit's game time during loads (a loadless comparison); real time keeps running"},

#define FLV_ROW_RUN_IN_BACKGROUND \
    {"run_in_background", settings::Bool, &coreframe::opts.runInBackground, 0, 1, \
     "1 = the game keeps running while its window is not in focus (Freelancer pauses otherwise); the mouse goes back " \
     "to Windows meanwhile"},

#define FLV_ROW_NPC_STALL_LOG \
    {"npc_stall_log", settings::Bool, &modslots::g_npcLog, 0, 1, \
     "writes EXE\\FLvibed_npc_stalls.log when a hostile ship near you stops moving: what the game says about it, for bug " \
     "reports; read only, it changes nothing in the game"},

#define FLV_ROW_FLUF_WINDOWS \
    {"fluf_windows", settings::Int, &coreframe::opts.flufWindows, 0, 2, \
     "with FLUF installed: 0 ignore its windows (co-residency only; FLvibed's keys still pass to FLUF while its " \
     "palette is open or its text box has the keys), 1 " \
     "its player windows (palette, options, map, toasts) are keep-outs: FLvibed's HUD parts and strip fade under " \
     "them, prompts and subtitles step away, the pad drives them, 2 (default) also its developer windows (the Debug " \
     "Window); FLUF's windows stay on top; no effect without FLUF"},

#define FLV_ROW_TEXT_SIZE \
    {"text_size", settings::Int, &coreframe::opts.textSize, 80, 150, \
     "size of FLvibed's own UI text (prompts, panels, the status line) in percent; the subtitles have their own"},

#define FLV_ROW_DECK_MODE \
    {"deck_mode", settings::Int, &coreframe::opts.deckMode, 0, 2, \
     "Steam Deck mode, larger text and no safe margins at the screen's edge: 0 automatic (on at 1280x800), 1 on, 2 off"},

// The subtitles' registry holds these; with the subtitles off the Movies module does (modules\movies\src\impl\movies.cpp).
#define FLV_ROW_MOVIE_FIT(value) \
    {"movie_fit", settings::Bool, value, 0, 0, \
     "movies keep their aspect ratio and the encoded black bars are zoomed out of view"},
#define FLV_ROW_MOVIE_SKIP(value) \
    {"movie_skip", settings::Int, value, 0, 1, "the startup movies: 0 play, 1 skip (natively, as a finished movie)"},
#define FLV_ROW_MOVIE_INTRO(value) \
    {"movie_intro", settings::Int, value, 0, 3, \
     "which intro plays: 0 as installed, 1 HD (fl_intro.wmv), 2 original 2003 (fl_intro_vanilla.wmv), 3 full Bonus " \
     "DVD (fl_intro_full.wmv), from DATA\\MOVIES - a file the install lacks falls back to 0"},
