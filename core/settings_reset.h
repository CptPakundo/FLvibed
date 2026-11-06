#pragma once
// The settings reset of a release that changes defaults (for 0.2.0: every FLvibed setting back
// to the current defaults when 0.2.0 first starts; HUD 2 became the default everywhere). Run by core\flvibed.cpp before
// any module reads its settings: if EXE\FLvibed.ini's [FLvibed] settings_version is below kVersion, FLvibed.ini,
// FLController.ini and FLSubtitles.ini are moved aside as <name>.before-<release> (an older backup of that name is
// replaced) and the stamp is written into a fresh FLvibed.ini; the modules then write their self-documenting ini files
// with the current defaults, as on a first install. The per-save modes and speedrun bests (EXE\FLvibed\) stay.
// The setup's new installs and the harness write the stamp with their FLvibed.ini (tools\setup\setup.cpp,
// tools\deploy.ps1), so the reset runs only for settings from an older release. The zip ships no FLvibed.ini: merged
// by hand it would take the player's stamp away and reset current settings. A later release that must reset again
// raises kVersion and kBackupSuffix together (and the two writers' stamp).
// Only macros, constants and an inline function (no state), so the header also works inside the build's namespaces.

namespace settings_reset
{
constexpr int kVersion = 2;                            // FLvibed.ini [FLvibed] settings_version
constexpr const char* kBackupSuffix = ".before-0.2.0"; // the moved-aside files' suffix
constexpr const char* kFiles[] = {"FLvibed.ini", "FLController.ini", "FLSubtitles.ini"};

// dir = the game's EXE folder with its trailing backslash. Returns the number of files moved aside, or -1 when the
// stamp is current (nothing done).
inline int Run(const char* dir)
{
    char ini[MAX_PATH];
    if (sprintf_s(ini, "%sFLvibed.ini", dir) < 0)
        return -1;
    if (GetPrivateProfileIntA("FLvibed", "settings_version", 0, ini) >= kVersion)
        return -1;
    int moved = 0;
    for (const char* name : kFiles)
    {
        char from[MAX_PATH], to[MAX_PATH];
        if (sprintf_s(from, "%s%s", dir, name) < 0 || sprintf_s(to, "%s%s%s", dir, name, kBackupSuffix) < 0)
            continue;
        if (GetFileAttributesA(from) != INVALID_FILE_ATTRIBUTES && MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING))
            ++moved;
    }
    char version[16];
    sprintf_s(version, "%d", kVersion);
    WritePrivateProfileStringA("FLvibed", "settings_version", version, ini);
    return moved;
}

// One value moved, not a reset: 0.3.5 raised `rumble`'s default from 60 to 100 and the first run writes every default to FLController.ini, so an untouched install
// holds 60 and would never see it (the owner, 6 Nov 2026). A stored 60 becomes 100, once per install: [FLvibed] settings_moved in FLvibed.ini says it ran, so a
// player who sets 60 afterwards keeps it. Any other value stays. Returns true when it moved the value. A later move raises kMoved.
constexpr int kMoved = 1;
inline bool MoveValues(const char* dir)
{
    char ini[MAX_PATH], controller[MAX_PATH];
    if (sprintf_s(ini, "%sFLvibed.ini", dir) < 0 || sprintf_s(controller, "%sFLController.ini", dir) < 0)
        return false;
    if (GetPrivateProfileIntA("FLvibed", "settings_moved", 0, ini) >= kMoved)
        return false;
    const bool moved = GetPrivateProfileIntA("Controller", "rumble", -1, controller) == 60;
    if (moved)
        WritePrivateProfileStringA("Controller", "rumble", "100", controller);
    char level[16];
    sprintf_s(level, "%d", kMoved);
    WritePrivateProfileStringA("FLvibed", "settings_moved", level, ini);
    return moved;
}
} // namespace settings_reset
