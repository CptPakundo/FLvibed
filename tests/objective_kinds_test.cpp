// The objective-kind coverage table (modules\autopilot\src\objective_kinds.h, IDEAS A41): the table itself (unique rows,
// names), then coverage over the install - every Cnd_ a story script's [Trigger] uses has a row whose `uses` equals the
// install's count, no row claims uses the install does not have, every vignetteparams.ini objective_text kind has a job
// row; an unknown name has no row (the coverage fails closed).
#include "test.h"

#include "../modules/autopilot/src/objective_kinds.h"
#include "game/ini.h"
#include "game/jobs.h"
#include "str.h"

#include <map>
#include <set>
#include <windows.h>

static std::vector<std::string> MissionScripts(const std::string& root)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA((root + "\\M*").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE)
        return out;
    std::vector<std::string> dirs;
    do
        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            dirs.push_back(root + "\\" + d.cFileName);
    while (FindNextFileA(h, &d));
    FindClose(h);
    for (const std::string& dir : dirs)
    {
        HANDLE f = FindFirstFileA((dir + "\\*.ini").c_str(), &d);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        do
            out.push_back(dir + "\\" + d.cFileName);
        while (FindNextFileA(f, &d));
        FindClose(f);
    }
    return out;
}

void Test_objective_kinds()
{
    using namespace objkinds;
    // ---- the table
    std::set<std::string> names;
    for (const CndRow& r : kCnd)
    {
        CHECK(names.insert(r.cnd).second);
        CHECK(str::Lower(r.cnd) == r.cnd && std::string(r.cnd).compare(0, 4, "cnd_") == 0);
        CHECK(r.type != GoalType::None && r.type != GoalType::Unknown && r.uses >= 0 && r.today && *r.today);
        CHECK(CndOf(r.cnd) == &r);
    }
    for (const JobRow& r : kJob)
    {
        CHECK(names.insert(r.kind).second);
        CHECK(JobOf(r.kind) == &r && r.type != GoalType::Unknown);
    }
    CHECK(kCndCount == 44 && kJobCount == 16);
    CHECK(CndOf("Cnd_DistShip") == CndOf("cnd_distship") && CndOf("cnd_nope") == nullptr && CndOf(nullptr) == nullptr);
    CHECK(JobOf("Destroy_loot") != nullptr && JobOf("kill_everything") == nullptr);
    for (int i = 0; i < static_cast<int>(GoalType::COUNT); ++i)
        CHECK(GoalTypeFromName(GoalTypeName(static_cast<GoalType>(i))) == static_cast<GoalType>(i));
    CHECK(GoalTypeFromName("nope") == GoalType::Unknown);
    if (!NeedInstall("the install's mission scripts and vignetteparams.ini"))
        return;
    // ---- coverage: every Cnd_ of the story scripts
    std::map<std::string, int> uses;
    int lines = 0;
    for (const std::string& path : MissionScripts(g_test.install + "\\DATA\\MISSIONS"))
        for (const ini::Section& sec : ini::Read(path))
            if (sec.type == "trigger")
                for (const auto& kv : sec.kv)
                    if (kv.first.compare(0, 4, "cnd_") == 0)
                        ++uses[kv.first], ++lines;
    int missing = 0, wrong = 0, unused = 0;
    for (const auto& [cnd, n] : uses)
    {
        const CndRow* r = CndOf(cnd.c_str());
        if (!r)
            ++missing, fprintf(stderr, "  objective_kinds: %s (%d uses) has no row\n", cnd.c_str(), n);
        else if (r->uses != n)
            ++wrong, fprintf(stderr, "  objective_kinds: %s uses %d in the install, the table says %d\n", cnd.c_str(), n, r->uses);
    }
    for (const CndRow& r : kCnd)
        if (!uses.count(r.cnd))
        {
            ++unused;
            if (r.uses != 0)
                ++wrong, fprintf(stderr, "  objective_kinds: %s unused in the install, the table says %d\n", r.cnd, r.uses);
        }
    CHECK(missing == 0);
    CHECK(wrong == 0);
    CHECK(!uses.empty());
    // ---- every job kind
    const jobs::Table jt = jobs::FromSections(ini::Read(g_test.install + "\\DATA\\RANDOMMISSIONS\\vignetteparams.ini"));
    CHECK(jt.ok);
    int jobMissing = 0;
    for (const std::string& k : jobs::Kinds(jt))
        if (!JobOf(k.c_str()))
            ++jobMissing, fprintf(stderr, "  objective_kinds: job kind %s has no row\n", k.c_str());
    CHECK(jobMissing == 0);
    printf("  coverage: %u Cnd_ used by the install (%d trigger lines), %d table rows (%d unused there), %d missing; %u job "
           "kinds (%u rows), %d missing\n",
           static_cast<unsigned>(uses.size()), lines, kCndCount, unused, missing,
           static_cast<unsigned>(jobs::Kinds(jt).size()), static_cast<unsigned>(jt.rows.size()), jobMissing);
}
