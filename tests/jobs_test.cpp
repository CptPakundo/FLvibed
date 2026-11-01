// The jobs' objective texts (core\game\jobs.h, IDEAS A40): the parser over synthetic sections (a unique id, an id two
// kinds share, an unused id, rows without an id), then the install's vignetteparams.ini (327734 -> guide_to_main_battle,
// 327757 shared -> ambiguous).
#include "test.h"

#include "game/jobs.h"

void Test_jobs()
{
    {
        std::vector<ini::Section> secs(3);
        secs[0].type = "datanode";
        secs[0].kv = {{"node_id", "95"}, {"objective_text", "guide_to_main_battle, 327734, Hostile_group"}};
        secs[1].type = "datanode";
        secs[1].kv = {{"node_id", "6"},
                      {"objective_text", "guide_to_main_battle, 327757"},
                      {"objective_text", "Jettison_Loot, 327757"},
                      {"objective_text", "return_loot, 327756, CRITICAL_LOOT, OFFER_BASE"},
                      {"objective_text", "broken"},
                      {"objective_text", "no_id, 0"}};
        secs[2].type = "decisionnode";
        secs[2].kv = {{"node_id", "7"}, {"nickname", "Big_solar"}};
        const jobs::Table t = jobs::FromSections(secs);
        CHECK(t.ok && t.rows.size() == 4);
        CHECK(t.rows[0].node == 95 && t.rows[0].args.size() == 1 && t.rows[0].args[0] == "Hostile_group");
        CHECK(t.rows[3].args.size() == 2 && t.rows[3].kind == "return_loot");
        int rows = 0;
        std::vector<std::string> kinds;
        CHECK(jobs::KindOf(t, 327734, &rows) == "guide_to_main_battle" && rows == 1);
        CHECK(jobs::KindOf(t, 327757, &rows, &kinds) == "ambiguous" && rows == 2 && kinds.size() == 2);
        CHECK(kinds.size() == 2 && kinds[1] == "jettison_loot"); // (kinds lower case)
        CHECK(jobs::KindOf(t, 12345, &rows).empty() && rows == 0);
        CHECK(jobs::KindOf(t, 0).empty());
        CHECK(jobs::Kinds(t).size() == 3);
        CHECK(!jobs::FromSections({}).ok);
    }
    if (!NeedInstall("the install's vignetteparams.ini"))
        return;
    const jobs::Table t = jobs::FromSections(ini::Read(g_test.install + "\\DATA\\RANDOMMISSIONS\\vignetteparams.ini"));
    CHECK(t.ok);
    int rows = 0;
    std::vector<std::string> kinds;
    CHECK(jobs::KindOf(t, 327734, &rows) == "guide_to_main_battle" && rows == 1);
    CHECK(jobs::KindOf(t, 327757, &rows, &kinds) == "ambiguous");
    const int shared = static_cast<int>(kinds.size());
    CHECK(shared > 1);
    CHECK(jobs::KindOf(t, 327745, &rows, &kinds) == "ambiguous"); // pb_kill_ships and mb_kill_ships
    printf("  vignetteparams.ini: %u objective_text rows, %u kinds; 327757 shared by %d kinds (%d rows)\n",
           static_cast<unsigned>(t.rows.size()), static_cast<unsigned>(jobs::Kinds(t).size()), shared,
           (jobs::KindOf(t, 327757, &rows), rows));
}
