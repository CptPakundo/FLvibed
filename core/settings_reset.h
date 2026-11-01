#pragma once
// The settings reset of a release that changes defaults (the owner, 1 Nov 2026, for 0.2.0: every FLvibed setting back
// to the current defaults when 0.2.0 first starts; HUD 2 became the default everywhere). Run by core\flvibed.cpp before
// any module reads its settings: if EXE\FLvibed.ini's [FLvibed] settings_version is below kVersion, FLvibed.ini,
// FLController.ini and FLSubtitles.ini are moved aside as <name>.before-<release> (an older backup of that name is
// replaced) and the stamp is written into a fresh FLvibed.ini; the modules then write their self-documenting ini files
// with the current defaults, as on a first install. The per-save modes and speedrun bests (EXE\FLvibed\) stay.
// The setup's new installs and the harness write the stamp with their FLvibed.ini (tools\setup\setup.cpp,
// tools\deploy.ps1), so the reset runs only for settings from an older release; the zip's FLvibed.ini has none on
// purpose (merged over old files by hand it must not skip the reset). A later release that must reset again raises
// kVersion and kBackupSuffix together (and the two writers' stamp).
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
} // namespace settings_reset
