// The controller's settings: the options table over core\settings.h, FLController.ini, `set` and `config`, the `ap` command's forward.
#include "controller.h"

#include "dimouse.h"
#include "game/events.h"
#include "game/aim.h"
#include "game/flightio.h"
#include "game/pursuit.h"
#include "settings.h"
#include "game/fl.h"
#include "game/ai.h"
#include "game/deckmode.h"
#include "game/flight.h"
#include "game/gameclock.h"
#include "game/geom.h"
#include "game/mission.h" // ForgetGame at a game load (host v34)
#include "game/render.h" // OnDeviceLost: the shared renderer's state block before a Reset
#include "game/room.h"
#include "game/space.h"
#include "game/spawn.h"
#include "log.h"
#include "../shm.h"
#include "device.h"
#include "gamepad.h"
#include "ui/glyphs.h"
#include "haptics.h" // the pad's vibration: events -> effect table -> mixer -> the two motors (IDEAS U30)
// phase D flight modals (the D-FLIGHT track): the goal panel is the Autopilot module's (core\ui\views.h),
#include "picker.h" // the target picker,
#include "wheel.h"  // the action wheel
#include "host.h"
#include "padhud.h"
#include "enabled_gate.h"
#include "game/coreframe.h"
#include "game/gamemode.h"
#include "game/mprules.h"
#include "ui/flufaware.h" // FLUF's windows: keep-outs, the pad on them, the keys' yield (docs\MOD_COMPATIBILITY.md FLUF)
// D-MENUS: the menu screens on the pad, the New Game prompt, speedrun mode (clock, splits, LiveSplit)
// and the HUD timer (padmenus.h / newgame.h; the clock and the timer: modules\speedrun)
#include "game/target_cycle.h"   // the targeting press's enemy rule
#include "newgame.h"
#include "padmenus.h"
#include "padbases.h" // phase D bases / dealers / NeuroNet windows (the D-BASES track),
#include "padmap.h"   // the nav map,
#include "wgroups.h"  // the weapon group editor,
#include "game/hudinfo.h"
#include "game/modslots.h" // the cut modules' slots
#include "promptbar.h" // the shared prompt bar (D-MENUS)
#include "ui/presets.h"
#include "press_gate.h" // press ownership between the pad's consumers (the attended run's B leak)
#include "ui/options_page.h"
#include "ui/ui.h"
#include "ui/uiopt.h"
#include "ui/hud_rows.h"
#include "ui/shared_rows.h"
#include "ui/views.h"

#include <deque>
#include <map>
#include <sstream>

#include "controller_internal.h"

namespace ctl
{
// ---------------------------------------------------------------- options table / FLController.ini
// Every tunable lives here once (the registry type core\settings.h; the autopilot's rows are autopilot_settings.cpp
// kDefs): `set <name> <value>` (live, not saved), `config save` (writes all current values to FLController.ini next to
// the DLL, a help line per setting), `config reload`, `config settings` (every setting of both sections with its help).
// The file is created with defaults and comments the first time.
static const settings::Def kOptions[] = {
    {"aim_mode", settings::Int, &g_opt.aimMode, 0, 1,
     "1 = gimbal: the right stick only steers, the reticle locks onto the selected hostile target's lead "
     "cross while it is on screen; 0 = the reticle follows the right stick (with the aim assists below)"},
    FLV_ROW_AIM_CONE(g_opt)
    {"aim_smooth", settings::Float, &g_opt.aimSmooth, 0, 1, "reticle smoothing time constant in seconds (0 = off)"},
    {"auto_target", settings::Int, &g_opt.autoTarget, 0, 2,
     "0 = off (default): you pick targets (the target button: what is under the reticle, else the closest enemy) "
     "and a selected target is never "
     "changed; 1 = select the closest enemy when the current one is destroyed; 2 = also when nothing is "
     "targeted (paused for 10 s after you clear the target yourself)"},
    {"fire_target", settings::Bool, &g_opt.fireTarget, 0, 1,
     "1 = with nothing targeted, firing at a hostile or neutral ship under the reticle selects it"},
    {"steer_expo", settings::Float, &g_opt.steerExpo, 1, 4, "steering response curve exponent (1 = linear)"},
    FLV_ROW_STEER_SCALE(g_opt)
    FLV_ROW_INVERT_Y(g_opt)
    {"stick_deadzone", settings::Float, &g_pad.stickDeadzone, 0, 0.9f, "radial stick deadzone (0..1)"},
    {"trigger_threshold", settings::Float, &g_pad.triggerThreshold, 0, 0.9f, "trigger deadzone (0..1)"},
    {"pad_backend", settings::Int, &g_pad.padBackend, 0, 1,
     "how the controller is read: 0 XInput (default; Xbox pads and pads that act as one), with a PlayStation or Switch pad "
     "read natively through SDL3 beside it (`pad_sdl_fallback`); 1 SDL3 always, the button prompts from the pad's type "
     "(it starts on its own thread in a few milliseconds, XInput reads the pad meanwhile and if SDL cannot start)"},
    {"pad_sdl_fallback", settings::Bool, &g_pad.sdlFallback, 0, 1,
     "read a PlayStation or Switch pad natively, through SDL3, with Steam Input's or another mapper's output for that pad dropped; off hands the pad to "
     "Steam Input or the mapper: FLvibed then reads XInput alone. Steam Deck and Steam Controller are not touched: Steam Input is their native route"},
    {"throttle_rate", settings::Float, &g_opt.throttleRate, 0.1f, 5,
     "throttle lever change per second at full left stick"},
    {"cursor_speed", settings::Float, &g_opt.cursorSpeed, 0.1f, 5, "menu cursor speed, screen heights per second"},
    {"wheel_rate", settings::Float, &g_opt.wheelRate, 1, 50, "right stick scrolling, notches per second"},
    {"aim_cursor", settings::Bool, &g_opt.aimCursor, 0, 1, "aim_mode 0: the reticle follows the right stick"},
    {"aim_assist", settings::Float, &g_opt.aimAssist, 0, 1,
     "aim_mode 0: pull of the reticle onto a hostile's lead cross"},
    {"aim_radius", settings::Float, &g_opt.aimRadius, 0.01f, 2, "aim_mode 0: pull radius, fraction of screen height"},
    {"aim_slow_radius", settings::Float, &g_opt.aimSlowRadius, 0.01f, 2, "aim_mode 0: steering slowdown radius"},
    {"aim_slowdown", settings::Float, &g_opt.aimSlowdown, 0, 1, "aim_mode 0: steering slowdown at the cross"},
    {"aim_rotation", settings::Float, &g_opt.aimRotation, 0, 1, "aim_mode 0: rotational assist strength"},
    {"aim_rotation_max", settings::Float, &g_opt.aimRotationMax, 0, 1, "aim_mode 0: rotational assist cap"},
    {"focus_only", settings::Bool, &g_opt.focusOnly, 0, 1,
     "ignore the controller while the game is in the background, in flight and menus alike (a button held when the "
     "game comes back is ignored until it is released)"},
    FLV_ROW_MANEUVER_WATCHDOG(g_opt)
    FLV_ROW_RUN_IN_BACKGROUND
    FLV_ROW_NPC_STALL_LOG
    FLV_HUD_ROWS_CUES
    FLV_ROW_AUTOPILOT_CANCEL(g_opt)
    // The UI build's rows (docs\UI_SPEC.json; the page shows them with their words, core\ui_meta.h)
    {"input_mode", settings::Int, &g_opt.inputMode, 0, 2,
     "input device: 0 automatic (the last device used wins, the UI follows in one frame), 1 controller, 2 keyboard "
     "and mouse"},
    {"glyph_style", settings::Int, &g_opt.glyphStyle, 0, 4,
     "button prompts: 0 automatic (the connected pad's family), 1 Xbox, 2 PlayStation, 3 Nintendo, 4 Steam (Steam Deck / "
     "Steam Controller: Xbox's face buttons, L1 / L2 / R1 / R2)"},
    {"glyph_look", settings::Int, &uiopt::Get().glyphLook, 0, 2,
     "how the button prompts look: 0 automatic (HUD 2 in its own colour with the face buttons in their platform's colours, the classic HUD and the menus in "
     "the buttons' colours), 1 coloured everywhere, 2 HUD 2 style everywhere; sizes stay as each screen has them"},
    {"confirm_button", settings::Int, &g_opt.confirmButton, 0, 1,
     "which face button accepts in menus: 0 the bottom one (A / cross / B), 1 the right one (Nintendo)"},
    {"pause_on_disconnect", settings::Bool, &g_opt.pauseOnDisconnect, 0, 1,
     "a pad that drops out mid-flight pauses the game; in menus the UI switches to the keyboard"},
    {"pad_preset", settings::Int, &g_opt.padPreset, 0, 3,
     "the flight layout: 0 Freelancer (targets and weapons on the fingers, consumables on the D-pad), 1 Ace (arcade "
     "flight: the left stick steers, the triggers are the throttle), 2 Assisted (the autopilot flies, you aim and "
     "decide), 3 Custom"},
    {"weapon_groups", settings::Int, &g_opt.weaponGroups, 0, 2,
     "0 simple: RT fires the active group, Y is cruise; 1 cycle: Y tap flips to the next group, Y held is cruise; 2 two "
     "groups: RT fires group 1 and LT group 2 (afterburner moves to the LS click), Y is cruise. Launchers fire only on "
     "their own buttons"},
    {"y_swap", settings::Bool, &g_opt.swapY, 0, 1,
     "the Freelancer layout with weapon_groups 1: Y tap is cruise and Y held flips to the next group, instead of the "
     "other way round"},
    {"holds_as_toggles", settings::Bool, &g_opt.holdsAsToggles, 0, 1,
     "afterburner, reverse, drift and rear view become press-once / press-again instead of held buttons"},
    {"rumble", settings::Int, &g_opt.rumble, 0, 100,
     "vibration strength in percent (0 = off), last in the mix: the ship, weapons, hits and warnings, set pieces, the "
     "autopilot's hand-back pulse and the game's boxes that wait for your answer; only while the controller is in use "
     "and the game is in front, never in true cinematics or the menus, and the world's effects never while paused"},
    {"trigger_effects", settings::Int, &g_opt.triggerEffects, 0, 2,
     "what the triggers do: 0 off, 1 light, 2 full. A DualSense's triggers feel weight, vibration and a wall when you "
     "cannot fire; an Xbox pad's trigger motors pulse (on USB or its dongle; over Bluetooth Windows is reported to "
     "drop them). A DualSense Edge's Medium and Short stops turn the effects off"},
    {"trigger_strength", settings::Int, &g_opt.triggerStrength, 0, 100,
     "the trigger effects' strength in percent, on top of trigger_effects"},
    {"haptics_mode", settings::Int, &g_opt.hapticsMode, 0, 2,
     "0 automatic (the waveform on a USB DualSense, the motors elsewhere), 1 motors, 2 waveform: a USB DualSense's grips driven as audio, "
     "its speaker set to four channels in the Sound panel; any failure falls back to motors and says why"},
    {"haptics_ship", settings::Int, &g_opt.hapticsClass[0], 0, 100,
     "percent: the engine, afterburner, cruise charge, tractor, nebulas and asteroids"},
    {"haptics_weapons", settings::Int, &g_opt.hapticsClass[1], 0, 100, "percent: your guns, missiles, mines and countermeasures"},
    {"haptics_danger", settings::Int, &g_opt.hapticsClass[2], 0, 100,
     "percent: hits from where they came, missiles, collisions, blasts, radiation, a failing hull"},
    {"haptics_setpieces", settings::Int, &g_opt.hapticsClass[3], 0, 100, "percent: trade lanes, jump gates and holes, docking and launching"},
    {"haptics_prompts", settings::Int, &g_opt.hapticsClass[4], 0, 100, "percent: the autopilot's hand-back and the game's boxes"},
    {"haptics_device", settings::Int, &g_opt.hapticsDevice, 0, 1,
     "0: nothing beyond a stop reaches a real pad, debug `haptics record` alone sees the output (a background game is always 0)"},
    {"pad_cursor", settings::Int, &g_opt.padCursor, 0, 2,
     "a cursor on the left stick where pointing is natural: 0 off, 1 rooms and maps, 2 everywhere; it slows over "
     "targets, the D-pad hides it"},
    {"cursor_magnet", settings::Int, &g_opt.cursorMagnet, 0, 100,
     "how strongly the pad cursor slows over and pulls toward a clickable element, percent"},
    {"prompt_bar", settings::Int, &g_opt.promptBar, 0, 2,
     "the bottom-right row of button prompts on controller screens: 0 contextual (every window, menu and modal while it is open; a base room when new or a decision is pending), "
     "1 always, 2 off"},
    FLV_ROW_FLUF_WINDOWS
    FLV_ROW_BORDERLESS(g_opt)
    {"world_prompts", settings::Int, &g_opt.worldPrompts, 0, 1,
     "a prompt by the target when something unusual is possible now (dock in range, loot in reach): 0 contextual, 1 "
     "off"},
    FLV_ROW_AP_HINTS(g_opt)
    FLV_ROW_TEXT_SIZE
    FLV_ROW_HUD_SIZE
    FLV_HUD_ROWS_TEXT
    {"hud_glyphs", settings::Bool, &g_opt.hudGlyphs, 0, 1,
     "in pad mode the HUD's own buttons carry the glyph of the button that acts on them"},
    {"button_legend", settings::Int, &g_opt.buttonLegend, 0, 1,
     "the contextual list of the actions available now with their buttons, right-middle of the screen: 0 = on, "
     "1 = off (the row's order: ON / OFF)"},
    {"ap_status_line", settings::Bool, &g_opt.apStatusLine, 0, 1,
     "the one line under the maneuver bar while the autopilot flies, cruise charges or turret view is on"},
    FLV_ROW_SHOW_ADVANCED(g_opt)
    FLV_ROW_SPEEDRUN_TIMER(g_opt)
    FLV_ROW_LIVESPLIT(g_opt)
    FLV_ROW_LIVESPLIT_PORT(g_opt)
    FLV_ROW_LIVESPLIT_GAMETIME(g_opt)
    FLV_ROW_AUTOPILOT_KEY(g_opt)
    // HUD 2 (modules\hud\docs\UI_HUD2.md 5.3; the options page's CONTROLLER tab, section HUD)
    FLV_HUD_ROWS_HUD
    FLV_ROW_MANEUVER_BUTTON(g_opt)
    FLV_HUD_ROWS_STYLE
};

static const size_t kOptionCount = sizeof(kOptions) / sizeof(kOptions[0]);
static_assert(sizeof(kOptions) / sizeof(kOptions[0]) <= kSettingsBusMaxRows, "the bus would refuse the whole table");

// ---------------------------------------------------------------- the settings bus (the PAGE track)
// core\settings_bus.h (HostApi v18): the [Controller] and [Autopilot] registries registered with the bus BEFORE the ini
// loads (impl.cpp ImplInit: RegisterBus, then Init -> LoadConfig), so the bus's default snapshot = the struct
// initialisers; the options page lists, changes, resets and saves them through it. Unregistered at Shutdown (the
// pointers die with the logic DLL). Without the bus (an older host) nothing changes: `set` / `ap set` and the ini stay.
static void SaveConfig();
static int g_busController = 0, g_busAutopilot = 0;
static void BusSave(void*) { SaveConfig(); }
static void BusAppliedController(void*, const settings::Def*) {} // `set` has no reactions beyond the value itself
static void BusAppliedAutopilot(void*, const settings::Def* d)
{
    if (views::autopilot.SettingApplied)
        views::autopilot.SettingApplied(d);
}
static SettingsBusSection g_busSections[2];
static const SettingsBus* g_bus = nullptr; // the host's, or the page's local one (an older host)

void RegisterBus()
{
    const SettingsBus* bus = HostAtLeast(18) && g_host->GetSettingsBus ? g_host->GetSettingsBus() : nullptr;
    if (!bus)
        bus = options_page::LocalBus();
    g_bus = bus;
    size_t n = 0;
    const settings::Def* apDefs = views::autopilot.Defs ? views::autopilot.Defs(n) : nullptr;
    g_busSections[0] = {"controller", "Controller", kOptions, kOptionCount, nullptr, BusAppliedController, BusSave,
                        nullptr};
    g_busSections[1] = {"controller", "Autopilot", apDefs, n, nullptr, BusAppliedAutopilot, BusSave, nullptr};
    g_busController = bus->Register(&g_busSections[0]);
    if (apDefs) // the Autopilot module's rows, with their file
        g_busAutopilot = bus->Register(&g_busSections[1]);
    Log("settings bus: Controller %d (%u rows), Autopilot %d (%u rows)", g_busController,
        static_cast<unsigned>(kOptionCount), g_busAutopilot, static_cast<unsigned>(n));
}

void UnregisterBus()
{
    if (g_bus)
    {
        if (g_busController)
            g_bus->Unregister(g_busController);
        if (g_busAutopilot)
            g_bus->Unregister(g_busAutopilot);
    }
    g_busController = g_busAutopilot = 0;
    g_bus = nullptr;
}

bool SetAndSave(const char* key, const char* value)
{
    return g_bus && g_bus->Set("Controller", key, value) && g_bus->Save("Controller");
}

static std::string ConfigPath()
{
    return std::string(g_host && g_host->dllDir ? g_host->dllDir : "") + "FLController.ini";
}

static void SaveConfig()
{
    // Rewrite the whole file (keeps the comments and the option order), both sections.
    std::vector<char> keptAutopilot(65536, 0);
    if (!views::autopilot.WriteCommented)
        GetPrivateProfileSectionA("Autopilot", keptAutopilot.data(), static_cast<DWORD>(keptAutopilot.size()), ConfigPath().c_str());
    FILE* f = nullptr;
    if (fopen_s(&f, ConfigPath().c_str(), "w") != 0 || !f)
    {
        Log("config: cannot write %s", ConfigPath().c_str());
        return;
    }
    fprintf(f, "; FLController - native gamepad support for Freelancer.\n");
    fprintf(f, "; Edit while the game is closed (or use the debug command \"config reload\").\n\n");
    settings::WriteCommented(f, "Controller", kOptions, kOptionCount);
    fprintf(f, "; Advanced Autopilot (modules\\autopilot\\docs\\AUTOPILOT.md; `ap settings` lists these in the game).\n");
    if (views::autopilot.WriteCommented)
        views::autopilot.WriteCommented(f);
    else // its module is off: its rows stay as the player left them
    {
        fprintf(f, "[Autopilot]\n");
        for (const char* row = keptAutopilot.data(); *row; row += strlen(row) + 1)
            fprintf(f, "%s\n", row);
    }
    fclose(f);
    Log("config: saved %s", ConfigPath().c_str());
}

void LoadConfig()
{
    const std::string path = ConfigPath();
    if (GetFileAttributesA(path.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        SaveConfig(); // first run: write the defaults with comments
        return;
    }
    settings::Load("Controller", path, kOptions, kOptionCount);
    if (views::autopilot.Load)
        views::autopilot.Load(path);
    Log("config: loaded %s", path.c_str());
}

std::string SetOption(std::istream& in)
{
    std::string name, value;
    in >> name >> value;
    if (name == "enabled")
    {
        coreframe::opts.enabled = atof(value.c_str()) != 0;
        if (!coreframe::opts.enabled)
            ReleaseAll(); // hand steering, held commands and the cursor back to the game
        return "ok enabled=" + value;
    }
    const settings::Def* d = settings::Find(kOptions, kOptionCount, name);
    if (!d)
        return "unknown option " + name;
    if (value.empty())
        return name + "=" + settings::Value(*d);
    if (!settings::Apply(*d, value))
        return "bad value for " + name + ": " + value + " (kept " + settings::Value(*d) + ")";
    return "ok " + name + "=" + settings::Value(*d) + " (live; 'config save' to keep)";
}

std::string AutopilotCommand(std::istream& in) { return views::ApCommand(in, ConfigPath()); }

std::string ConfigCommand(std::istream& in)
{
    std::string what;
    in >> what;
    if (what == "save")
        SaveConfig();
    else if (what == "reload")
        LoadConfig();
    else if (what == "settings") // every setting of both sections with its value and help (the registry)
        return "[Controller]\n" + settings::Reference(kOptions, kOptionCount) + "\n[Autopilot]\n" +
               (views::autopilot.SettingsReference ? views::autopilot.SettingsReference() : std::string());
    return ConfigPath() + "\n" + settings::Describe(kOptions, kOptionCount, " = ", "\n") + "\n";
}
} // namespace ctl
