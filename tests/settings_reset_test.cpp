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

    // 0.3.5's rumble: a stored 60 moves to 100, once
    const std::string pad = dir + "FLController.ini";
    const auto rumble = [&pad] { return GetPrivateProfileIntA("Controller", "rumble", -1, pad.c_str()); };
    CHECK(WriteTextFile(ini, "[FLvibed]\r\nsettings_version = 2\r\n"));
    CHECK(WriteTextFile(pad, "; comment\r\n[Controller]\r\nhud_style = 1\r\nrumble = 60\r\n"));
    CHECK(settings_reset::MoveValues(dir.c_str()));
    CHECK(rumble() == 100);
    CHECK(GetPrivateProfileIntA("Controller", "hud_style", 0, pad.c_str()) == 1);                              // the rest of the file stays
    CHECK(GetPrivateProfileIntA("FLvibed", "settings_version", 0, ini.c_str()) == settings_reset::kVersion); // no reset
    // a second start: a 60 the player chose since stays
    CHECK(WriteTextFile(pad, "[Controller]\r\nrumble = 60\r\n"));
    CHECK(!settings_reset::MoveValues(dir.c_str()));
    CHECK(rumble() == 60);
    // 0.4.0: deck_mode moves into hud_size and goes, a hud_size already there stays, 0.3.5's move does not run again
    const auto size = [&pad] { return GetPrivateProfileIntA("Controller", "hud_size", -1, pad.c_str()); };
    const auto deck = [&pad] { return GetPrivateProfileIntA("Controller", "deck_mode", -1, pad.c_str()); };
    for (const int from : {0, 1, 2})
    {
        char file[96];
        sprintf_s(file, "[Controller]\r\nrumble = 60\r\ndeck_mode = %d\r\n", from);
        CHECK(WriteTextFile(ini, "[FLvibed]\r\nsettings_version = 2\r\nsettings_moved = 1\r\n"));
        CHECK(WriteTextFile(pad, file));
        CHECK(settings_reset::MoveValues(dir.c_str()));
        CHECK(size() == (from == 1 ? 3 : from == 2 ? 2 : 0));
        CHECK(deck() == -1);
        CHECK(rumble() == 60);
    }
    CHECK(WriteTextFile(ini, "[FLvibed]\r\nsettings_version = 2\r\nsettings_moved = 1\r\n"));
    CHECK(WriteTextFile(pad, "[Controller]\r\ndeck_mode = 1\r\nhud_size = 1\r\n"));
    CHECK(settings_reset::MoveValues(dir.c_str()));
    CHECK(size() == 1);
    // any other value stays, and the move is spent
    CHECK(WriteTextFile(ini, "[FLvibed]\r\nsettings_version = 2\r\n"));
    CHECK(WriteTextFile(pad, "[Controller]\r\nrumble = 45\r\n"));
    CHECK(!settings_reset::MoveValues(dir.c_str()));
    CHECK(rumble() == 45);
    CHECK(GetPrivateProfileIntA("FLvibed", "settings_moved", 0, ini.c_str()) == settings_reset::kMoved);
    // a new install: no FLController.ini yet, the move spent all the same, so a 60 set later stays
    DeleteFileA(pad.c_str());
    CHECK(WriteTextFile(ini, "[FLvibed]\r\nsettings_version = 2\r\n"));
    CHECK(!settings_reset::MoveValues(dir.c_str()));
    CHECK(WriteTextFile(pad, "[Controller]\r\nrumble = 60\r\n"));
    CHECK(!settings_reset::MoveValues(dir.c_str()));
    CHECK(rumble() == 60);
}
