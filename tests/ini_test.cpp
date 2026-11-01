// core\game\ini.cpp: sections in order, keys lower case, values as written (trimmed), repeated keys kept, comments and
// blank lines dropped, a missing file empty; Fields splits on commas and trims.
#include "test.h"

#include "game/ini.h"
#include "str.h"

void Test_ini()
{
    const std::string path = TempPath("ini_test.ini");
    CHECK(WriteTextFile(path, "; a comment line\r\n"
                              "key_before_any_section = ignored\r\n"
                              "[Trigger]\r\n"
                              "nickname = First ; trailing comment\r\n"
                              "System = Li01\r\n"
                              "Act_ActTrig = a_b\r\n"
                              "  act_acttrig =   c_d  \r\n"
                              "\r\n"
                              "no equals sign here\r\n"
                              "[NNObjective]\r\n"
                              "Nickname=nn_x\r\n"
                              "type = navmarker, Li01, 24260, 27260 ,1,2 , 3\r\n"
                              "[Empty]\r\n"
                              "[Trigger]\r\n"
                              "nickname = Second\r\n"));
    const std::vector<ini::Section> s = ini::Read(path);
    CHECK(s.size() == 4);
    if (s.size() == 4)
    {
        CHECK(s[0].type == "trigger");
        CHECK(s[1].type == "nnobjective");
        CHECK(s[2].type == "empty");
        CHECK(s[3].type == "trigger");
        // first section: 4 pairs, keys lower, values trimmed, the comment stripped, order kept
        CHECK(s[0].kv.size() == 4);
        const std::string* nick = s[0].Get("nickname");
        CHECK(nick && *nick == "First");
        const std::string* sys = s[0].Get("system");
        CHECK(sys && *sys == "Li01");         // value case kept
        CHECK(s[0].Get("System") == nullptr); // keys are matched lower case
        CHECK(s[0].Get("missing") == nullptr);
        const std::vector<std::string> acts = s[0].All("act_acttrig");
        CHECK(acts.size() == 2);
        CHECK(acts.size() == 2 && acts[0] == "a_b" && acts[1] == "c_d");
        CHECK(s[0].All("nothing").empty());
        // second section
        CHECK(s[1].kv.size() == 2);
        const std::string* type = s[1].Get("type");
        CHECK(type != nullptr);
        if (type)
        {
            const std::vector<std::string> f = ini::Fields(*type);
            CHECK(f.size() == 7);
            if (f.size() == 7)
            {
                CHECK(f[0] == "navmarker");
                CHECK(f[1] == "Li01");
                CHECK(f[3] == "27260");
                CHECK(f[4] == "1");
                CHECK(f[5] == "2");
                CHECK(f[6] == "3");
            }
        }
        CHECK(s[2].kv.empty());
        const std::string* second = s[3].Get("nickname");
        CHECK(second && *second == "Second");
    }
    // Fields: always at least one field; empty fields kept
    CHECK(ini::Fields("").size() == 1 && ini::Fields("")[0].empty());
    const std::vector<std::string> f = ini::Fields("a,,b ,");
    CHECK(f.size() == 4);
    CHECK(f.size() == 4 && f[0] == "a" && f[1].empty() && f[2] == "b" && f[3].empty());
    // a missing file reads as nothing
    CHECK(ini::Read(TempPath("does_not_exist.ini")).empty());
    DeleteFileA(path.c_str());

    // a binary ini (BINI, vanilla 1.1's DATA files) reads as the same sections (29 Oct 2026; it read as empty):
    // [Sound] nickname = mix_s051a / file = audio\mixes\mix_s051xa.wav, [Zone] pos = -1200, 0.5, 7 / damage = 2000000
    {
        const std::string strings[] = {"Sound", "nickname", "mix_s051a", "file", "audio\\mixes\\mix_s051xa.wav",
                                       "Zone", "pos", "damage"};
        std::string table;
        unsigned off[8];
        for (int i = 0; i < 8; ++i)
            off[i] = static_cast<unsigned>(table.size()), table += strings[i], table += '\0';
        std::string b = "BINI";
        auto u16 = [&](unsigned v) { b += static_cast<char>(v & 0xFF), b += static_cast<char>((v >> 8) & 0xFF); };
        auto u32 = [&](unsigned v) { u16(v & 0xFFFF), u16(v >> 16); };
        auto value = [&](int type, unsigned raw) { b += static_cast<char>(type), u32(raw); };
        u32(1);          // version
        u32(0);          // the string table's offset: patched below
        u16(off[0]), u16(2);                                   // [Sound], 2 entries
        u16(off[1]), b += '\x01', value(3, off[2]);            // nickname = "mix_s051a"
        u16(off[3]), b += '\x01', value(3, off[4]);            // file = "audio\mixes\..."
        u16(off[5]), u16(2);                                   // [Zone], 2 entries
        float half = 0.5f;
        unsigned halfRaw;
        memcpy(&halfRaw, &half, 4);
        u16(off[6]), b += '\x03', value(1, static_cast<unsigned>(-1200)), value(2, halfRaw), value(1, 7);
        u16(off[7]), b += '\x01', value(1, 2000000);
        const unsigned strOff = static_cast<unsigned>(b.size());
        for (int i = 0; i < 4; ++i)
            b[8 + i] = static_cast<char>((strOff >> (8 * i)) & 0xFF);
        b += table;
        const std::string bin = TempPath("ini_test_bini.ini");
        CHECK(WriteTextFile(bin, b));
        const std::vector<ini::Section> bs = ini::Read(bin);
        CHECK(bs.size() == 2);
        if (bs.size() == 2)
        {
            CHECK(bs[0].type == "sound" && bs[1].type == "zone");
            const std::string *nick = bs[0].Get("nickname"), *file = bs[0].Get("file");
            CHECK(nick && *nick == "mix_s051a" && file && *file == "audio\\mixes\\mix_s051xa.wav");
            const std::string *pos = bs[1].Get("pos"), *dmg = bs[1].Get("damage");
            CHECK(pos && *pos == "-1200, 0.5, 7" && dmg && *dmg == "2000000");
            const std::vector<std::string> pf = pos ? ini::Fields(*pos) : std::vector<std::string>();
            CHECK(pf.size() == 3 && atof(pf[1].c_str()) == 0.5);
        }
        DeleteFileA(bin.c_str());
    }
    // the real thing when the repository's vanilla copy is there (git-ignored research\Vanilla, beside Install\)
    if (!g_test.install.empty())
    {
        const std::string vanilla = g_test.install + "\\..\\..\\research\\Vanilla\\Freelancer\\DATA\\Audio\\gf_sounds.ini";
        if (GetFileAttributesA(vanilla.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            std::string file;
            int sounds = 0;
            for (const ini::Section& sec : ini::Read(vanilla))
                if (sec.type == "sound")
                {
                    ++sounds;
                    if (const std::string* n = sec.Get("nickname"); n && *n == "mix_s051a" && sec.Get("file"))
                        file = *sec.Get("file");
                }
            CHECK(sounds > 50);
            CHECK(str::Lower(file) == "audio\\mixes\\mix_s051xa.wav");
        }
    }
}
