// The registry of the game's flight roots (modules\hud\docs\UI_HUD2_SKIN.md 3.1, UI_HUD2_INPUTS.md 3): one row per native root, so nothing
// native reaches the screen by default and nothing wanted goes by accident. Each row says hide, keep, keep the root with its art gone, or
// hidden with a home that says the same thing in HUD 2. A root with no row stays visible and is logged: a true deny would blank
// brackets, the banner and waypoints. Pure: hudvis.cpp walks the game's roots against it, tests\hud2_roots_test.cpp holds the rules.
#pragma once
#include <cstring>

namespace hudroots
{
enum Action : int
{
    Hide,      // hidden while HUD 2 shows
    Keep,      // stays the game's, on purpose
    KeepNoArt, // the root stays (a window lives under it), its art goes
    Home       // hidden: HUD 2 says the same thing in `home`
};

struct Row
{
    const char* name;
    Action action;
    const char* home; // where HUD 2 carries it, "" none
};

inline constexpr Row kRows[] = {
    {"HUD_Maneuvers", Hide, ""},
    {"HUD_Target", Hide, ""},
    {"HUD_ContactList", Hide, ""},
    {"HUD_Status", Hide, ""},
    {"HUD_Core", Hide, ""},
    {"HUD_CruiseProgress", Hide, ""}, // (its keep rule: the disrupted cruise line's seconds)
    {"HUD_DamageIndicator", Hide, ""},
    {"MsgWin", Hide, ""},
    {"NN_MainButton", KeepNoArt, ""},
    {"Targetable_Objects", Keep, ""},
    {"HUD_Waypoint", Keep, ""},
    {"NN_Scanner", Keep, ""},
    {"NN_GroupControl", Keep, ""},
    {"BriefingParentWindow", Keep, ""},
    {"LoadingProgressBar", Keep, ""},
    {"MissionObjectiveWindow", Keep, ""},
    {"CommText", Keep, ""},
    {"Rollover", Keep, ""},
    {"ToolTip", Keep, ""},
    {"FLvibedHud2Hotspots", Keep, ""},
    {"NN_ArrivalText", Home, "the nav's place line"},
    {"MissileAlert", Home, "the MISSILE INCOMING card and the ring's chevrons"},
    {"RadiationAlert", Home, "the nav's amber line"},
    {"TrailAlert", Home, "the ring's tail mark"},
    {"LagAlert", Home, "the nav's small word"},
};

// Below the kept `Targetable_Objects`: its parts by what HUD 2 does with them (6.10). The child alerts have rows above; `ReticleRangeText` keeps drawing what the
// mute leaves: off-screen names, group names.
inline constexpr Row kTargetParts[] = {
    {"corners", Home, "the targeting display's caret"},
    {"label", Home, "the targeting display's words"},
    {"bars", Home, "the targeting display's bars"},
};

inline const Row* Find(const char* name)
{
    if (!name)
        return nullptr;
    for (const Row& r : kRows)
        if (strcmp(r.name, name) == 0)
            return &r;
    return nullptr;
}

inline const char* ActionName(const Row* r)
{
    if (!r)
        return "unknown";
    switch (r->action)
    {
    case Hide: return "hide";
    case Keep: return "keep";
    case KeepNoArt: return "keep, art hidden";
    case Home: return "home";
    }
    return "?";
}
} // namespace hudroots
