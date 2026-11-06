// The modules with the controller off (alone.h). FLController.ini is not rewritten here: a save writes the rows of the modules that run, one by one, and every other
// row stays as it is. Each `[Controller]` row that acts on the game has its home here, with the module that uses it: on the bus, so the page shows it live.
#include "logic/alone.h"

#include "game/coreframe.h"
#include "game/debugverbs.h"
#include "game/flightio.h"
#include "game/gamemode.h"
#include "game/maneuver_watchdog.h"
#include "game/hostcalls.h"
#include "game/modslots.h"
#include "game/mprules.h"
#include "game/session.h"
#include "log.h"
#include "modules_rules.h"
#include "settings_bus.h"
#include "ui/hud_rows.h"
#include "ui/shared_rows.h"
#include "ui/uiopt.h"
#include "ui/views.h"

namespace alone
{
static bool g_on = false;
static std::string g_ini;
static SettingsBusSection g_apSection, g_hudSection, g_rowsSection;
static int g_apBus = 0, g_hudBus = 0, g_rowsBus = 0;
static sharedrows::Values g_rows;
static std::vector<settings::Def> g_defs; // the shared rows of what runs here

static const SettingsBus* Bus() { return hostcalls::g.GetSettingsBus ? hostcalls::g.GetSettingsBus() : nullptr; }

static void AppliedAutopilot(void*, const settings::Def* d)
{
    if (views::autopilot.SettingApplied)
        views::autopilot.SettingApplied(d);
}
static void AppliedHud(void*, const settings::Def*) {} // `set` has no reactions beyond the value itself

static void SaveRows(const char* section, const settings::Def* defs, size_t n)
{
    const SettingsBus* bus = Bus();
    if (!bus || !defs)
        return;
    char v[96];
    for (size_t i = 0; i < n; ++i)
        if (bus->Get(section, defs[i].name, v, sizeof(v)))
            WritePrivateProfileStringA(section, defs[i].name, v, g_ini.c_str());
}

static void SaveAutopilot(void*)
{
    size_t n = 0;
    SaveRows("Autopilot", views::autopilot.Defs ? views::autopilot.Defs(n) : nullptr, n);
}
static void SaveHud(void*) { SaveRows("Controller", hudrows::kRows, hudrows::kRowCount); }
static void SaveShared(void*) { SaveRows("Controller", g_defs.data(), g_defs.size()); }

// What the frame reads comes from the rows at once, as the controller's copy does each frame.
static void PublishRows()
{
    flightio::Get() = {g_rows.steerScale, g_rows.aimCone, g_rows.invertY, g_rows.autopilotCancel, g_rows.maneuverWatchdog};
    uiopt::Get().apHints = g_rows.apHints;
}
static void AppliedShared(void*, const settings::Def*) { PublishRows(); }

template <size_t N> static void AddRows(const settings::Def (&rows)[N]) { g_defs.insert(g_defs.end(), rows, rows + N); }

// Who uses a row owns it here: core's first, each module's with the module.
static void BuildRows()
{
    const settings::Def core[] = {FLV_ROW_MANEUVER_WATCHDOG(g_rows) FLV_ROW_FLUF_WINDOWS FLV_ROW_TEXT_SIZE FLV_ROW_DECK_MODE FLV_ROW_SHOW_ADVANCED(g_rows)};
    AddRows(core);
    const auto on = [](int row) { return hostcalls::g.ModuleOn && hostcalls::g.ModuleOn(row); };
    if (on(modules::kDisplay))
    {
        const settings::Def rows[] = {FLV_ROW_RUN_IN_BACKGROUND FLV_ROW_BORDERLESS(g_rows)};
        AddRows(rows);
    }
    if (on(modules::kNpc))
    {
        const settings::Def rows[] = {FLV_ROW_NPC_STALL_LOG};
        AddRows(rows);
    }
    if (on(modules::kSpeedrun))
    {
        const settings::Def rows[] = {FLV_ROW_SPEEDRUN_TIMER(g_rows) FLV_ROW_LIVESPLIT(g_rows) FLV_ROW_LIVESPLIT_PORT(g_rows) FLV_ROW_LIVESPLIT_GAMETIME(g_rows)};
        AddRows(rows);
    }
    if (on(modules::kAutopilot))
    {
        const settings::Def rows[] = {FLV_ROW_STEER_SCALE(g_rows) FLV_ROW_AIM_CONE(g_rows) FLV_ROW_INVERT_Y(g_rows) FLV_ROW_AUTOPILOT_CANCEL(g_rows)
                                      FLV_ROW_AP_HINTS(g_rows) FLV_ROW_AUTOPILOT_KEY(g_rows) FLV_ROW_MANEUVER_BUTTON(g_rows)};
        AddRows(rows);
    }
}

void Init(const char* dllDir)
{
    const bool autopilot = views::ApPresent(), hud = views::HudPresent();
    g_on = autopilot || hud || (hostcalls::g.ModuleOn && hostcalls::g.ModuleOn(modules::kSpeedrun));
    g_ini = std::string(dllDir) + "FLController.ini";
    const SettingsBus* bus = Bus();
    // (before the load: the bus's defaults are the values as they start)
    g_defs.clear();
    BuildRows();
    if (bus)
    {
        g_rowsSection = {"core", "Controller", g_defs.data(), g_defs.size(), nullptr, AppliedShared, SaveShared, nullptr};
        g_rowsBus = bus->Register(&g_rowsSection);
        if (!g_rowsBus)
            Log("core: its rows are not on the settings bus");
    }
    settings::Load("Controller", g_ini, g_defs.data(), g_defs.size());
    PublishRows();
    if (!g_on)
        return;
    session::Init();
    if (autopilot)
    {
        size_t n = 0;
        const settings::Def* defs = views::autopilot.Defs(n);
        if (bus)
        {
            g_apSection = {"autopilot", "Autopilot", defs, n, nullptr, AppliedAutopilot, SaveAutopilot, nullptr};
            g_apBus = bus->Register(&g_apSection);
            if (!g_apBus)
                Log("autopilot: its rows are not on the settings bus");
        }
        views::autopilot.Load(g_ini);
        Log("autopilot: alone, on keyboard and mouse (%u rows)", static_cast<unsigned>(n));
    }
    if (hud)
    {
        if (bus)
        {
            g_hudSection = {"hud", "Controller", hudrows::kRows, hudrows::kRowCount, nullptr, AppliedHud, SaveHud, nullptr};
            g_hudBus = bus->Register(&g_hudSection);
            if (!g_hudBus)
                Log("hud: its rows are not on the settings bus");
        }
        settings::Load("Controller", g_ini, hudrows::kRows, hudrows::kRowCount);
        Log("hud: alone, on keyboard and mouse (%u rows)", static_cast<unsigned>(hudrows::kRowCount));
    }
}

void Shutdown()
{
    if (const SettingsBus* bus = Bus())
    {
        if (g_apBus)
            bus->Unregister(g_apBus);
        if (g_hudBus)
            bus->Unregister(g_hudBus);
        if (g_rowsBus)
            bus->Unregister(g_rowsBus);
    }
    g_apBus = g_hudBus = g_rowsBus = 0;
    if (!g_on)
        return;
    session::Shutdown();
    views::BarShutdown();
    flightio::ReleaseAll();
    coreframe::Stop();
    g_on = false;
}

bool On() { return g_on; }

bool Command(const std::string& op, std::istream& in, std::string& reply)
{
    if (op == "watchdog")
    {
        reply = mwatch::Command(in);
        return true;
    }
    if (!g_on)
        return false;
    if (op == "ap")
        reply = views::ApCommand(in, g_ini);
    else if (op == "owners")
        reply = "owners:" + flightio::HeldOwners();
    else if (op == "mp")
        reply = mprules::Describe();
    else if (op == "core") // core <verb ...>: the debug verbs of core\game\debugverbs.cpp, for a test with no controller's channel
    {
        std::string rest;
        std::getline(in >> std::ws, rest);
        reply = debug::Execute(rest);
    }
    else if (op == "mode")
        reply = gamemode::Name(coreframe::g_mode);
    else if (op == "fault") // as debugverbs.cpp
        reply = coreframe::FaultCommand(in);
    else
        return modslots::Command(op, in, reply);
    return true;
}
} // namespace alone
