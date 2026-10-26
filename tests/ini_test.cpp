// core\game\ini.cpp: sections in order, keys lower case, values as written (trimmed), repeated keys kept, comments and
// blank lines dropped, a missing file empty; Fields splits on commas and trims.
#include "test.h"

#include "game/ini.h"

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
}
