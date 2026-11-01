// core\game\mission.cpp's decisions without the game, through mission::DecisionSweep (the format of
// tools\decision_baseline.txt): a synthetic script with a known answer, and the install's DATA\MISSIONS against the
// baseline the Python port (tools\resolver_check.py) produced - the two implementations check each other. The goal order
// inside a line differs (the port iterates a set), so lines are compared with their goals sorted.
#include "test.h"

#include "game/mission.h"

#include <algorithm>
#include <set>

// A baseline line with its goal tuples sorted: "<head> -> [<tuple>, <tuple>]".
static std::string Normalize(const std::string& line)
{
    const size_t arrow = line.find(" -> [(");
    if (arrow == std::string::npos || line.size() < 2 || line.compare(line.size() - 2, 2, ")]") != 0)
        return line;
    std::vector<std::string> tuples;
    std::string body = line.substr(arrow + 6, line.size() - arrow - 6 - 2); // between "[(" and ")]"
    size_t s = 0;
    for (;;)
    {
        const size_t c = body.find("), (", s);
        tuples.push_back(body.substr(s, c == std::string::npos ? std::string::npos : c - s));
        if (c == std::string::npos)
            break;
        s = c + 4;
    }
    std::sort(tuples.begin(), tuples.end());
    std::string out = line.substr(0, arrow) + " -> [";
    for (size_t i = 0; i < tuples.size(); ++i)
        out += (i ? ", (" : "(") + tuples[i] + ")";
    return out + "]";
}

void Test_decision_synthetic()
{
    const std::string path = TempPath("m99.ini", "M99");
    CHECK(WriteTextFile(path, "[Mission]\n"
                              "string_id = 24000\n"
                              "\n"
                              "[NNObjective]\n"
                              "nickname = nn_go\n"
                              "type = navmarker, Li01, 24260, 27260, 0, 0, 0\n"
                              "\n"
                              "[NNObjective]\n"
                              "nickname = nn_done\n"
                              "type = ids, 24270\n"
                              "\n"
                              "[MsnSolar]\n"
                              "nickname = gen1\n"
                              "label = generators\n"
                              "\n"
                              "[MsnSolar]\n"
                              "nickname = gen2\n"
                              "label = generators\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = start\n"
                              "system = Li01\n"
                              "Cnd_True = no_params\n"
                              "Act_SetNNObj = nn_go\n"
                              "Act_ActTrig = arrive\n"
                              "Act_ActTrig = too_far\n"
                              "Act_ActTrig = gens_down\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = arrive\n"
                              "Cnd_DistShip = inside, Player, Li01_01_Base, 2000\n"
                              "Act_SetNNObj = nn_done\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = too_far\n"
                              "Cnd_DistShip = outside, Player, Li01_01_Base, 50000\n"
                              "Act_ChangeState = FAIL\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = gens_down\n"
                              "Cnd_Destroyed = generators, 2\n"
                              "Act_SetNNObj = nn_done\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = done\n"
                              "system = Li01\n"
                              "Cnd_True = no_params\n"
                              "Act_SetNNObj = nn_done\n"
                              "Act_ActTrig = finish\n"
                              "Act_ActTrig = wrong_system\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = finish\n"
                              "Cnd_DistVec = inside, Player, 100, 200, 300, 500\n"
                              "Act_ChangeState = SUCCEED\n"
                              "\n"
                              "[Trigger]\n"
                              "nickname = wrong_system\n"
                              "system = Li02\n"
                              "Cnd_DistVec = inside, Player, 1, 2, 3, 400\n"
                              "Act_ChangeState = SUCCEED\n"));
    std::vector<std::string> lines = mission::DecisionSweep(g_test.temp);
    std::sort(lines.begin(), lines.end());
    for (std::string& l : lines)
        l = Normalize(l);
    if (g_test.verbose)
        for (const std::string& l : lines)
            printf("  %s\n", l.c_str());
    CHECK(lines.size() == 2);
    if (lines.size() == 2)
    {
        // nn_go: the step's chain arms arrive (-> nn_done) and gens_down (2 of the labelled solars -> nn_done); too_far
        // (outside) is no goal, FAIL chains are not followed
        CHECK(lines[1] ==
              Normalize("m99.ini nn_go [24260] li01 -> [('arrive', 'player', 'Li01_01_Base 2000 m', 'nn_done'), "
                        "('gens_down', 'destroy', '2 of gen1,gen2', 'nn_done')]"));
        // nn_done: its setters (arrive, gens_down, done) and the trigger that armed them (start) are the roots; what they
        // arm: finish succeeds, wrong_system runs in another system
        CHECK(
            lines[0] ==
            Normalize("m99.ini nn_done [24270] li01 -> [('finish', 'player', 'point 100,200,300 500 m', 'SUCCEED')]"));
    }
    // a folder without scripts
    CHECK(mission::DecisionSweep(TempPath("", "empty")).empty());
    DeleteFileA(path.c_str());
}

// A new game forgets AdvanceGoals' memory (mission::ForgetGame, called at HostApi v34 GameLoads; A37 S1, 30 Oct 2026):
// two synthetic missions - M98 shows its step, then a load of M97 shows only "Awaiting" (21660) under M97's title. Before
// the forget M98's last objective stands in (the stale stand-in S1 found: M07's for M12); after it the title names M97.
void Test_decision_forget_game()
{
    const std::string root = TempPath("", "LOADS"); // (TempPath makes one folder level: the parent first)
    const std::string a = TempPath("m98.ini", "LOADS\\M98"), b = TempPath("m97.ini", "LOADS\\M97");
    CHECK(WriteTextFile(a, "[NNObjective]\nnickname = nn_x\ntype = ids, 24360\n\n"
                           "[Trigger]\nnickname = start\nsystem = Li01\nCnd_True = no_params\nAct_SetTitle = 24800\n"
                           "Act_SetNNObj = nn_x\nAct_ActTrig = arrive\n\n"
                           "[Trigger]\nnickname = arrive\nCnd_DistShip = inside, Player, Li01_01_Base, 2000\n"
                           "Act_ChangeState = SUCCEED\n"));
    CHECK(WriteTextFile(b, "[NNObjective]\nnickname = nn_y\ntype = ids, 24460\n\n"
                           "[Trigger]\nnickname = start\nsystem = Li01\nCnd_True = no_params\nAct_SetTitle = 24900\n"
                           "Act_SetNNObj = nn_y\nAct_ActTrig = arrive\n\n"
                           "[Trigger]\nnickname = arrive\nCnd_DistShip = inside, Player, Li01_02_Base, 2000\n"
                           "Act_ChangeState = SUCCEED\n"));
    mission::UseScriptsFrom(root);
    const u32 li01 = 0, awaiting = 21660;
    auto file = [](const std::string& path) { return path.substr(path.find_last_of("\\/") + 1); };
    bool fromLast = true;
    mission::AdvanceGoals(24360, li01, &fromLast, 24800); // M98's step: remembered
    CHECK(!fromLast);
    mission::AdvanceInfo info;
    mission::AdvanceGoalsPeek(awaiting, li01, 24900, info); // the same game: M98's last step stands in
    CHECK(info.fromLast && !info.byTitle && info.standInText == 24360 && file(info.script) == "m98.ini");
    mission::ForgetGame(); // a load
    mission::AdvanceGoalsPeek(awaiting, li01, 24900, info);
    CHECK(info.fromLast && info.byTitle && info.standInText == 0 && file(info.script) == "m97.ini");
    mission::AdvanceGoalsPeek(awaiting, li01, 0, info); // no title yet (the host forgets it at a load too): nothing
    CHECK(!info.fromLast && !info.byTitle && info.script.empty());
    mission::ForgetGame(); // (a second forget is harmless)
    mission::AdvanceGoalsPeek(24460, li01, 24900, info); // the loaded game's own step
    CHECK(!info.fromLast && file(info.script) == "m97.ini");
    mission::UseScriptsFrom(""); // back to the tests' default: no scripts
    DeleteFileA(a.c_str());
    DeleteFileA(b.c_str());
}

void Test_decision_baseline()
{
    if (!NeedInstall("the decision baseline sweep"))
        return;
    const std::vector<std::string> base = ReadLines(g_test.baseline);
    if (base.empty())
    {
        ++g_test.skipped;
        printf("  skipped: no baseline at %s\n", g_test.baseline.c_str());
        return;
    }
    const std::vector<std::string> now = mission::DecisionSweep(g_test.install + "\\DATA\\MISSIONS");
    CHECK(!now.empty());
    std::set<std::string> a, b;
    for (const std::string& l : base)
        if (!l.empty())
            a.insert(Normalize(l));
    for (const std::string& l : now)
    {
        const std::string n = Normalize(l);
        if (a.count(n))
            a.erase(n);
        else
            b.insert(n);
    }
    // a = baseline lines the C++ did not produce, b = C++ lines not in the baseline
    int shown = 0;
    for (const std::string& l : a)
        if (shown++ < 10)
            fprintf(stderr, "  <= baseline only: %s\n", l.c_str());
    shown = 0;
    for (const std::string& l : b)
        if (shown++ < 10)
            fprintf(stderr, "  => C++ only:      %s\n", l.c_str());
    if (g_test.verbose || !a.empty() || !b.empty())
        printf("  %u baseline lines, %u from the C++ sweep, %u / %u differ\n", static_cast<unsigned>(base.size()),
               static_cast<unsigned>(now.size()), static_cast<unsigned>(a.size()), static_cast<unsigned>(b.size()));
    CHECK(a.empty());
    CHECK(b.empty());
}
