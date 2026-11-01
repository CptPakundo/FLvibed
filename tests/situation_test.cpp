// The situation assessor (controller\src\impl\situation.h, A37 S1): over a synthetic snapshot - the subject, the kind and
// its source, threats, constraints, goals with live state and achieved, affordances; idempotence and contact-order
// invariance; the step key from the snapshot alone (4c4fb6f: a re-issued step keeps its key, "Dock with the Trade Lane"
// after the escort gets a new one - no memory needed); placeholders and stand-ins; holds; the job table's precedence;
// the verb rules. Then the recorded fixtures (tests\fixtures\situation\*.snap and -snaps <dir>): the host-free hash of
// each equals the hash the game computed for that very snapshot (#game-hash), and their #expect lines hold.
#include "situation_fixture.h"

#include <algorithm>
#include <map>

using namespace situation;

static std::map<std::string, std::string> Summary(const Situation& s)
{
    return {{"placeholder", PlaceholderName(s.placeholder)},
            {"standin", StandInName(s.standIn)},
            {"kind", objkinds::GoalTypeName(s.kind)},
            {"source", KindSourceName(s.kindSource)},
            {"subject", SubjectSourceName(s.subjectSource)},
            {"held", HeldName(s.held)},
            {"verb", VerbName(s.verb)}};
}

static std::string Threats(const Situation& s)
{
    std::string o;
    for (const Threat& t : s.threats)
        o += (o.empty() ? "" : " ") + std::to_string(t.id) + ":" + ThreatWhyName(t.why);
    return o;
}

void Test_situation()
{
    // ---- the verb rules (the objective mode's, with their regressions)
    CHECK(VerbOf("Destroy the Rheinland Agents") == Verb::Fight); // ("land " inside "Rheinland": no dock)
    CHECK(VerbOf("Dock with the Trade Lane") == Verb::Dock);
    CHECK(VerbOf("Enter Formation with the USV Brandt") == Verb::Escort);
    CHECK(VerbOf("Head to the Barrier") == Verb::None); // ("bar" a whole word only)
    CHECK(VerbOf("Meet Juni in the Manhattan Bar") == Verb::Dock);
    CHECK(VerbOf("Defend Yourself!") == Verb::Survive);
    CHECK(VerbOf("Defend Quintaine and Sinclair") == Verb::Escort);
    CHECK(VerbOf("Kill all %F0v2 ships!") == Verb::Fight);
    CHECK(VerbOf("") == Verb::None && VerbOf("Fly to the Waypoint") == Verb::None);
    for (Verb v : {Verb::None, Verb::Dock, Verb::Escort, Verb::Fight, Verb::Survive})
        CHECK(VerbFromName(VerbName(v)) == v);

    // ---- the synthetic snapshot
    const Snapshot s = SyntheticSnapshot();
    const Situation a = Assess(s);
    auto sum = Summary(a);
    CHECK(sum["placeholder"] == "none" && a.known && sum["standin"] == "none");
    CHECK(sum["subject"] == "ref" && a.subject == 101 && a.subjectHasPos && a.subjectDist == 1000);
    CHECK(sum["kind"] == "escort" && sum["source"] == "script" && a.kindDetail == "rep_inst+friendly_ship");
    CHECK(sum["held"] == "none");
    CHECK(Threats(a) == "202:pointed 505:pre_hostile 303:battle");
    CHECK(a.constraints.size() == 2 && a.constraints[0].kind == Constraint::Kind::Leash && a.constraints[0].armed == -1 &&
          a.constraints[1].kind == Constraint::Kind::Nag);
    CHECK(a.goals.size() == 3);
    if (a.goals.size() == 3)
    {
        CHECK(a.goals[0].source == "ini" && a.goals[0].cnd == "cnd_distship" && a.goals[0].live == 0 &&
              a.goals[0].achieved == Achieved::Yes && a.goals[0].distance == 500);
        CHECK(a.goals[1].cnd == "cnd_destroyed" && a.goals[1].live == 2 && a.goals[1].achieved == Achieved::Yes &&
              a.goals[1].distance == 20000); // (606 alive, 607 gone: 1 of count 1)
        CHECK(a.goals[2].source == "next" && a.goals[2].live == -1 && a.goals[2].achieved == Achieved::No);
    }
    CHECK(a.liveArmed == 1 && !a.afford.dock && a.afford.lane == -2);
    CHECK(a.stepKey == LegacyStepKey(s.step) && a.stepKey == (23270u * 2654435761u ^ 8u));
    CHECK(a.hash != 0 && Describe(a).find("situation v1 hash=0x") == 0);

    // ---- idempotence, contact-order invariance
    {
        const Situation b = Assess(s);
        CHECK(b.hash == a.hash && Describe(b) == Describe(a));
        Snapshot r = s;
        std::reverse(r.contacts.begin(), r.contacts.end());
        CHECK(Assess(r).hash == a.hash && Describe(Assess(r)) == Describe(a));
        for (size_t k = 1; k < s.contacts.size(); ++k)
        {
            Snapshot q = s;
            std::rotate(q.contacts.begin(), q.contacts.begin() + static_cast<long>(k), q.contacts.end());
            CHECK(Assess(q).hash == a.hash);
        }
        Snapshot m = s; // the marks' order does not name another step
        std::reverse(m.marks.begin(), m.marks.end());
        CHECK(Assess(m).step == a.step);
        Snapshot t = s; // facts the assessor does not read (the clock, measurements) do not move the hash
        t.clock += 100, t.liveMs = 9, t.objSeq += 3;
        CHECK(Assess(t).hash == a.hash);
    }

    // ---- the step key from the snapshot alone (4c4fb6f's sequence: escort, the escort re-issued, "Dock with the Trade
    // Lane" - the guard swallowed the third decision and the stored key took it for the same step)
    const auto sequence = [](const Snapshot& escort, u32 dockText, u32 laneRef, u32 laneObj)
    {
        Snapshot s2 = escort;
        s2.objSeq += 1; // re-issued: same text, target, navmarker
        Snapshot s3 = s2;
        s3.objSeq += 1;
        s3.step = SnapStep();
        s3.step.textId = dockText, s3.step.verb = Verb::Dock;
        if (laneRef)
            s3.step.refs[0] = laneRef, s3.step.refObj[0] = laneObj, s3.step.refCount = 1;
        const Situation k1 = Assess(escort), k2 = Assess(s2), k3 = Assess(s3);
        CHECK(k1.step == k2.step && k1.stepKey == k2.stepKey);
        CHECK(k3.step != k1.step && k3.stepKey != k1.stepKey);
        return k3;
    };
    {
        Snapshot dockDefs = s;
        dockDefs.objDefs = {{"nn_dock_lane", "rep_inst", "M07\\m07.ini"}};
        const Situation k3 = sequence(dockDefs, 23265, 9, 404);
        CHECK(objkinds::GoalTypeName(k3.kind) == std::string("trade_lane") && k3.kindSource == KindSource::Script &&
              k3.kindDetail == "rep_inst+lane_ring" && k3.afford.lane == 1 && k3.afford.dock);
        Snapshot added = s; // a mark added is a new step (s12d M04), the legacy key alone does not see it
        added.marks.push_back(999);
        CHECK(Assess(added).stepKey == a.stepKey && Assess(added).step != a.step);
    }

    // ---- placeholders and stand-ins
    {
        Snapshot p = s;
        p.step = SnapStep();
        p.step.textId = 21660;
        p.objDefs.clear();
        Situation x = Assess(p);
        CHECK(Summary(x)["placeholder"] == "awaiting" && Summary(x)["standin"] == "none" && !x.known);
        // markFight (202 marked, hostile, 2 km): the verb becomes Fight - no mark is the subject then
        CHECK(Summary(x)["kind"] == "attack" && x.kindDetail == "mark_fight" && Summary(x)["verb"] == "fight");
        CHECK(Summary(x)["subject"] == "none");
        p.goalsByTitle = p.goalsFromLast = true; // ...and it comes before any stand-in (SaveS182: the Nomads, not Toledo)
        CHECK(Assess(p).kindDetail == "mark_fight");
        p.goalsByTitle = p.goalsFromLast = false;
        p.marks = {101};
        x = Assess(p);
        CHECK(Summary(x)["kind"] == "wait" && Summary(x)["source"] == "unknown" && x.kindDetail == "awaiting");
        CHECK(Summary(x)["subject"] == "mark" && x.subject == 101); // the newest live friendly mark
        p.goalsByTitle = p.goalsFromLast = true;
        x = Assess(p);
        CHECK(Summary(x)["standin"] == "title" && Summary(x)["source"] == "script" && Summary(x)["kind"] == "fly_to" &&
              x.kindDetail == "title+cnd_distship");
        // the placeholder's own [NNObjective]s (15 scripts' "mlog_awaiting_objective", "nn_objsoon") name no step
        p.objDefs = {{"nn_objsoon", "ids", "M01a\\m01a.ini"}, {"mlog_awaiting_objective", "navmarker", "M03\\m03.ini"}};
        x = Assess(p);
        CHECK(!x.known && x.kindDetail == "title+cnd_distship");
        p.objDefs.clear();
        p.goalsByTitle = false, p.standInText = 23250;
        x = Assess(p);
        CHECK(Summary(x)["standin"] == "ini_last" && x.standInText == 23250);
        p.marks.clear();
        p.goals.clear(), p.goalsFromLast = false, p.standInText = 0;
        x = Assess(p);
        CHECK(Summary(x)["kind"] == "wait" && Summary(x)["source"] == "unknown" && Summary(x)["subject"] == "none");
        p.step.textId = 0;
        x = Assess(p);
        CHECK(Summary(x)["placeholder"] == "no_objective" && Summary(x)["kind"] == "none" && Summary(x)["source"] == "none");
        p.step.textId = 1;
        CHECK(Summary(Assess(p))["placeholder"] == "blank");
        p.step.textId = 5555, p.objDefs = {{"nn_blank", "ids", "M13\\m13.ini"}};
        CHECK(Summary(Assess(p))["placeholder"] == "blank");
    }

    // ---- what holds the ship
    {
        const auto held = [&](void (*f)(Snapshot&)) { Snapshot h = s; f(h); return std::string(HeldName(Assess(h).held)); };
        CHECK(held([](Snapshot& h) { h.inSpace = false, h.base = 123; }) == "docked");
        CHECK(held([](Snapshot& h) { h.inSpace = false; }) == "no_ship");
        CHECK(held([](Snapshot& h) { h.missionHolds = true, h.laneRide = true; }) == "mission_holds");
        CHECK(held([](Snapshot& h) { h.sinceGrantMs = 1000; }) == "game_dock");
        CHECK(held([](Snapshot& h) { h.sinceGrantMs = 1000, h.ourDock = true; }) == "none");
        CHECK(held([](Snapshot& h) { h.sinceGrantMs = 9000; }) == "none");
        CHECK(held([](Snapshot& h) { h.sinceGrantMs = 9000, h.op = 7; }) == "game_dock");
        CHECK(held([](Snapshot& h) { h.dockInProgress = true; }) == "game_dock");
        CHECK(held([](Snapshot& h) { h.sinceGrantMs = 1000, h.contacts[0].type = 1u << 10; }) == "none"); // airlock fly-through
        CHECK(held([](Snapshot& h) { h.laneRide = true; }) == "lane");
        CHECK(held([](Snapshot& h) { h.jumping = true; }) == "jump");
    }

    // ---- the survival stance: only attackers close by (and the pre-aggro ships)
    {
        Snapshot v = s;
        v.survival = true;
        CHECK(Threats(Assess(v)) == "505:pre_hostile");
        v.contacts[1].distance = 900;
        CHECK(Threats(Assess(v)) == "202:attacker 505:pre_hostile");
    }

    // ---- the job table first; an ambiguous id falls back; a never-kill step is a constraint
    {
        Snapshot j = s;
        j.step.job = "guide_to_main_battle";
        Situation x = Assess(j);
        CHECK(Summary(x)["kind"] == "fly_to" && Summary(x)["source"] == "job" && x.kindDetail == "job:guide_to_main_battle");
        j.step.job = "";
        j.entries[1].job = "mb_dont_kill_ships";
        x = Assess(j);
        CHECK(Summary(x)["kind"] == "constraint" && Summary(x)["source"] == "job");
        CHECK(!x.constraints.empty() && x.constraints.back().kind == Constraint::Kind::NeverAttack);
        j.entries[1].job = "zzz";
        CHECK(Assess(j).kindDetail == "job:zzz:no_row" && Summary(Assess(j))["kind"] == "unknown");
        j.entries[1].job = "ambiguous", j.step.job = "ambiguous", j.objDefs.clear();
        x = Assess(j);
        CHECK(Summary(x)["source"] == "verb" && x.kindDetail == "verb:escort;job_ambiguous");
        Snapshot two = s; // a text two scripts' real objectives name is no known step
        two.objDefs.push_back({"nn_other", "ids", "M08\\m08.ini"});
        CHECK(!Assess(two).known && Assess(two).kindSource == KindSource::Verb);
    }

    // ---- the recorded fixtures: the host-free hash = the game's, the expectations hold
    int n = 0, repo = 0;
    for (const std::string& dir : {g_test.fixtures + "\\situation", g_test.snaps})
        for (const SnapFixture& f : dir.empty() ? std::vector<SnapFixture>() : SnapFixtures(dir))
        {
            Snapshot x;
            std::string err;
            if (!Parse(f.text, x, &err))
            {
                CHECK(false);
                fprintf(stderr, "  situation: %s: %s\n", f.name.c_str(), err.c_str());
                continue;
            }
            const Situation sit = Assess(x);
            const std::string game = SnapComment(f.text, "game-hash");
            const u32 want = static_cast<u32>(strtoul(game.c_str(), nullptr, 16));
            CHECK(!game.empty() && want == sit.hash);
            if (game.empty() || want != sit.hash)
                fprintf(stderr, "  situation: %s: game hash %s, host-free 0x%08x\n", f.name.c_str(), game.c_str(), sit.hash);
            CHECK(Assess(x).hash == sit.hash); // idempotent on recorded data
            Snapshot r = x;
            std::reverse(r.contacts.begin(), r.contacts.end());
            CHECK(Assess(r).hash == sit.hash); // contact order
            const std::map<std::string, std::string> got = Summary(sit);
            std::string ex = SnapComment(f.text, "expect");
            for (const std::string& kv : io::Split(ex, ' '))
            {
                const size_t eq = kv.find('=');
                const std::string k = kv.substr(0, eq), v = eq == std::string::npos ? "" : kv.substr(eq + 1);
                const auto it = got.find(k);
                const bool ok = it != got.end() && it->second == v;
                CHECK(ok);
                if (!ok)
                    fprintf(stderr, "  situation: %s: expected %s, got %s\n", f.name.c_str(), kv.c_str(),
                            it == got.end() ? "(no such field)" : it->second.c_str());
            }
            if (g_test.verbose)
                printf("  %s: hash 0x%08x %s %s %s %s subject=%s held=%s\n", f.name.c_str(), sit.hash,
                       got.at("placeholder").c_str(), got.at("standin").c_str(), got.at("kind").c_str(),
                       got.at("source").c_str(), got.at("subject").c_str(), got.at("held").c_str());
            ++n;
            repo += dir == g_test.fixtures + "\\situation" ? 1 : 0;
            // the 4c4fb6f sequence over the recorded M07 snapshot
            if (f.name.find("m07") != std::string::npos)
            {
                Snapshot esc = x;
                esc.step = SnapStep();
                esc.step.textId = 23270, esc.step.verb = Verb::Escort;
                sequence(esc, 23265, 0, 0);
            }
        }
    CHECK(repo >= 6); // the six fixtures of A37_PLAN S1
    printf("  %d recorded snapshots (%d in tests\\fixtures\\situation): hashes equal the game's\n", n, repo);
}
