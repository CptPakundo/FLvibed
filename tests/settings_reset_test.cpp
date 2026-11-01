// core\settings_reset.h: settings from an older release (no stamp) are moved aside as backups and the stamp written;
// a stamped folder is left alone; a fresh folder (no files) only gets the stamp; an old backup is replaced.
#include <windows.h>

#include "test.h"

#include "settings_reset.h"

static bool Exists(const std::string& path)
{
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

void Test_settings_reset()
{
    const std::string dir = TempPath("", "settings_reset");
    for (const char* name : settings_reset::kFiles)
    {
        DeleteFileA((dir + name).c_str());
        DeleteFileA((dir + name + settings_reset::kBackupSuffix).c_str());
    }

    // 0.1.0's files: no stamp -> all three moved aside, the stamp written into a fresh FLvibed.ini
    CHECK(WriteTextFile(dir + "FLvibed.ini", "[Modules]\r\nsubtitles = 0\r\ncontroller = 1\r\n"));
    CHECK(WriteTextFile(dir + "FLController.ini", "[Controller]\r\nhud_style = 1\r\n"));
    CHECK(WriteTextFile(dir + "FLSubtitles.ini", "[Subtitles]\r\nsize = 3\r\n"));
    CHECK(WriteTextFile(dir + "FLController.ini" + settings_reset::kBackupSuffix, "an older backup\r\n"));
    CHECK(settings_reset::Run(dir.c_str()) == 3);
    for (const char* name : {"FLController.ini", "FLSubtitles.ini"})
    {
        CHECK(!Exists(dir + name));
        CHECK(Exists(dir + name + settings_reset::kBackupSuffix));
    }
    const std::string ini = dir + "FLvibed.ini";
    CHECK(GetPrivateProfileIntA("FLvibed", "settings_version", 0, ini.c_str()) == settings_reset::kVersion);
    CHECK(GetPrivateProfileIntA("Modules", "subtitles", 1, ini.c_str()) == 1); // the old switch is gone: default on
    const std::string backup = dir + "FLController.ini" + settings_reset::kBackupSuffix;
    CHECK(GetPrivateProfileIntA("Controller", "hud_style", 0, backup.c_str()) == 1); // the old backup replaced

    // stamped: nothing moves, a setting written since stays
    CHECK(WriteTextFile(dir + "FLController.ini", "[Controller]\r\nhud_style = 0\r\n"));
    CHECK(settings_reset::Run(dir.c_str()) == -1);
    CHECK(Exists(dir + "FLController.ini"));

    // a fresh folder: nothing to move, the stamp written
    for (const char* name : settings_reset::kFiles)
        DeleteFileA((dir + name).c_str());
    CHECK(settings_reset::Run(dir.c_str()) == 0);
    CHECK(GetPrivateProfileIntA("FLvibed", "settings_version", 0, ini.c_str()) == settings_reset::kVersion);
}
