// The live trigger table as the story source (core\game\mission.h GoalsOfLive / ArmedLeashes / LeadsOfLive, A37 S2):
// a synthetic script and a hand-built LiveTable - goals only from armed rows (inactive / complete ones never fire), each
// condition parsed by AdvanceGoals' rules (flying, destroy, the system filter, the step's own objective, a failing
// chain), a leash only while its trigger is armed, every row's lead with its hop, the fallback when the table is not ok;
// then the situation assessor over a live snapshot (source live, the live stand-in, armed leashes, the live-vs-ini
// difference, the kind by the armed row's Cnd_) and a v1 snapshot unchanged; then the install's scripts over the recorded
// live rows of SaveS182 / Save3989 / SaveS208 (tests\fixtures\situation): S182 -> the Toledo goal, M13 docked -> exit_base
// (Cnd_BaseExit) leads at once, M07 -> no live goal where the ini view had one.
#include "situation_fixture.h"

#include "game/flhash.h"
#include "game/mission.h"
#include "game/mission_live.h"

#include <algorithm>

namespace
{
mission::LiveTrigger Row(const char* nick, int state)
{
    mission::LiveTrigger r;
    r.id = flhash::CreateId(nick);
    r.state = state;
    r.nickname = nick;
    return r;
}

const mission::Goal* Find(const std::vector<mission::Goal>& gs, const char* trigger)
{
    for (const mission::Goal& g : gs)
        if (g.trigger == trigger)
            return &g;
    return nullptr;
}

// The live table of a recorded fixture (its livetrig rows: ids and states; the script found by the ids).
bool LiveOfFixture(const std::string& name, mission::LiveTable& t, situation::Snapshot& snap)
{
    const std::string text = ReadTextFile(g_test.fixtures + "\\situation\\" + name);
    std::string err;
    if (!situation::Parse(text, snap, &err))
    {
        fprintf(stderr, "  mission_live_goals: %s: %s\n", name.c_str(), err.c_str());
        return false;
    }
    t = mission::LiveTable();
    t.ok = snap.liveOk;
    for (const situation::SnapLive& l : snap.live)
    {
        mission::LiveTrigger r;
        r.id = l.id, r.state = l.state, r.nickname = l.nick;
        t.triggers.push_back(r);
    }
    return true;
}
} // namespace

void Test_mission_live_goals()
{
    const std::string root = TempPath("", "LIVEG"); // (TempPath makes one folder level: the parent first)
    const std::string path = TempPath("m96.ini", "LIVEG\\M96");
    CHECK(WriteTextFile(path, "[NNObjective]\nnickname = nn_go\ntype = ids, 24510\n\n"
                              "[NNObjective]\nnickname = nn_next\ntype = ids, 24520\n\n"
                              "[NNObjective]\nnickname = nn_done\ntype = ids, 24530\n\n"
                              "[MsnSolar]\nnickname = gen1\nlabel = generators\n\n"
                              "[MsnSolar]\nnickname = gen2\nlabel = generators\n\n"
                              "[MsnShip]\nnickname = buddy\nNPC = buddy_npc\n\n"
                              "[NPC]\nnickname = buddy_npc\nindividual_name = 21001\n\n"
                              "[Trigger]\nnickname = start\nsystem = Li01\nCnd_True = no_params\nAct_SetNNObj = nn_go\n"
                              "Act_ActTrig = near_base\nAct_ActTrig = gens_down\nAct_ActTrig = to_point\n"
                              "Act_ActTrig = leash_buddy\nAct_ActTrig = comm_then\n\n"
                              "[Trigger]\nnickname = near_base\nCnd_DistShip = inside, Player, Li01_01_Base, 2000\n"
                              "Act_SetNNObj = nn_next\n\n"
                              "[Trigger]\nnickname = gens_down\nsystem = Li01\nCnd_Destroyed = generators, 2\n"
                              "Act_SetNNObj = nn_done\n\n"
                              "[Trigger]\nnickname = to_point\nsystem = Li01\nCnd_DistVec = inside, Player, 100, 0, 200, 500\n"
                              "Act_ChangeState = SUCCEED\n\n"
                              "[Trigger]\nnickname = leash_buddy\nsystem = Li01\nCnd_DistShip = outside, Player, buddy, 6000\n"
                              "Act_ChangeState = FAIL\n\n"
                              "[Trigger]\nnickname = other_sys\nsystem = Li02\nCnd_DistShip = inside, Player, Li02_01_Base, 1000\n"
                              "Act_SetNNObj = nn_next\n\n"
                              "[Trigger]\nnickname = repeat_current\nsystem = Li01\n"
                              "Cnd_DistShip = inside, Player, Li01_02_Base, 1000\nAct_SetNNObj = nn_go\n\n"
                              "[Trigger]\nnickname = exit_base\nCnd_BaseExit = li01_01_base\nAct_SetNNObj = nn_next\n\n"
                              "[Trigger]\nnickname = comm_then\nsystem = Li01\nCnd_CommComplete = dx_m96_0100\n"
                              "Act_ActTrig = near_base\n\n"
                              "[Trigger]\nnickname = fails\nsystem = Li01\nCnd_DistShip = inside, Player, Li01_03_Base, 500\n"
                              "Act_ChangeState = FAIL\n"));
    mission::UseScriptsFrom(root);
    const u32 li01 = flhash::CreateId("Li01");

    mission::LiveTable t;
    t.ok = true;
    t.script = path;
    t.triggers = {Row("near_base", 0),  Row("gens_down", 1),  Row("to_point", 2), Row("leash_buddy", 0),
                  Row("other_sys", 0),  Row("repeat_current", 0), Row("exit_base", 0), Row("comm_then", 0),
                  Row("fails", 0)};
    mission::LiveTrigger unknown;
    unknown.id = 12345, unknown.state = 0;
    t.triggers.push_back(unknown);

    // ---- goals: armed rows only, AdvanceGoals' parse
    std::vector<mission::Goal> g = mission::GoalsOfLive(t, li01, 24510);
    CHECK(g.size() == 1);
    const mission::Goal* nb = Find(g, "near_base");
    CHECK(nb && nb->kind == 1 && nb->object == flhash::CreateId("Li01_01_Base") && nb->dist == 2000 &&
          nb->subject == "player" && nb->leads == "nn_next" && !nb->byEntry && nb->file == path);
    CHECK(!Find(g, "gens_down") && !Find(g, "to_point")); // inactive / complete: never fire
    CHECK(!Find(g, "other_sys"));                         // Li02's
    CHECK(!Find(g, "repeat_current"));                    // back to the step's own objective
    CHECK(!Find(g, "exit_base") && !Find(g, "comm_then")); // no flying / destroy condition
    CHECK(!Find(g, "fails") && !Find(g, "leash_buddy"));   // a failing chain / an outside distance
    // (no step text: an objective the step already shows counts as an advance)
    CHECK(Find(mission::GoalsOfLive(t, li01, 0), "repeat_current"));
    // in Li02: only Li02's (near_base has no system of its own: its arming trigger's, Li01)
    g = mission::GoalsOfLive(t, flhash::CreateId("Li02"), 24510);
    CHECK(g.size() == 1 && Find(g, "other_sys"));
    // the same rows armed: a destroy goal and a point
    mission::LiveTable all = t;
    for (mission::LiveTrigger& r : all.triggers)
        r.state = 0;
    g = mission::GoalsOfLive(all, li01, 24510);
    CHECK(g.size() == 3);
    const mission::Goal* gd = Find(g, "gens_down");
    CHECK(gd && gd->kind == 3 && gd->count == 2 && gd->objects.size() == 2 && gd->leads == "nn_done");
    const mission::Goal* tp = Find(g, "to_point");
    CHECK(tp && tp->kind == 2 && tp->pos[0] == 100 && tp->pos[2] == 200 && tp->dist == 500 && tp->leads == "SUCCEED");
    CHECK(g.size() == 3 && g[0].trigger == "near_base" && g[1].trigger == "gens_down"); // (the table's order)

    // ---- a leash only while its trigger is armed (state 0)
    std::vector<mission::Leash> l = mission::ArmedLeashes(t, li01);
    CHECK(l.size() == 1 && l[0].object == flhash::CreateId("buddy") && l[0].dist == 6000 && l[0].nameIds == 21001);
    for (int state : {1, 2})
    {
        mission::LiveTable off = t;
        for (mission::LiveTrigger& r : off.triggers)
            if (r.nickname == "leash_buddy")
                r.state = state;
        CHECK(mission::ArmedLeashes(off, li01).empty());
    }
    CHECK(mission::ArmedLeashes(t, flhash::CreateId("Li02")).empty()); // (its system)
    mission::LiveTable gone = t; // not in the live list at all: not armed
    gone.triggers.erase(std::remove_if(gone.triggers.begin(), gone.triggers.end(),
                                       [](const mission::LiveTrigger& r) { return r.nickname == "leash_buddy"; }),
                        gone.triggers.end());
    CHECK(mission::ArmedLeashes(gone, li01).empty());
    // (the ini view keeps to every leash of the last script, armed or not - the S1 behaviour, unchanged)
    bool fromLast = false;
    mission::AdvanceGoals(24510, li01, &fromLast);
    CHECK(mission::Leashes(li01).size() == 1);

    // ---- every row's lead and its hop
    const std::vector<mission::LiveLead> leads = mission::LeadsOfLive(t, 24510);
    CHECK(leads.size() == t.triggers.size());
    if (leads.size() == t.triggers.size())
    {
        CHECK(leads[0].leads == "nn_next" && leads[0].hops == 0);  // near_base
        CHECK(leads[6].leads == "nn_next" && leads[6].hops == 0);  // exit_base: its own action
        CHECK(leads[7].leads == "nn_next" && leads[7].hops == 1);  // comm_then -> near_base
        CHECK(leads[8].leads == "FAIL" && leads[8].hops == 0);     // fails
        CHECK(leads[5].leads.empty() && leads[5].hops == -1);      // repeat_current: the step's own objective
        CHECK(leads[9].leads.empty() && leads[9].hops == -1);      // an id the script lacks
    }

    // ---- the script found by the ids when the table names none
    mission::LiveTable noPath = t;
    noPath.script.clear();
    CHECK(mission::GoalsOfLive(noPath, li01, 24510).size() == 1);

    // ---- the fallback: a table that is not ok gives nothing (the caller takes the ini view)
    mission::LiveTable bad = t;
    bad.ok = false;
    bad.reason = "no Content.dll";
    CHECK(mission::GoalsOfLive(bad, li01, 24510).empty() && mission::ArmedLeashes(bad, li01).empty());
    CHECK(mission::LeadsOfLive(bad, 24510).size() == bad.triggers.size() && mission::LeadsOfLive(bad)[0].leads.empty());
    mission::LiveTable foreign = t; // ids no script knows
    for (mission::LiveTrigger& r : foreign.triggers)
        r.id ^= 0x5A5A5A5A;
    foreign.script.clear();
    CHECK(mission::GoalsOfLive(foreign, li01, 24510).empty());
    mission::UseScriptsFrom("");
    DeleteFileA(path.c_str());

    // ---- the assessor over a live snapshot
    using namespace situation;
    {
        Snapshot s = SyntheticSnapshot();
        CHECK(s.version == kSnapVersion && kSnapVersion >= 2);
        // live: near_ring armed (both views), gens_dead only in the ini view, a live-only goal; the leash armed
        s.goalsLive = true;
        s.iniGoals = s.goals;
        SnapGoal only = s.goals[0];
        only.trigger = "live_only", only.leads = "nn_x";
        s.goals = {s.goals[0], only};
        s.iniLeashes = s.leashes;
        s.iniLeashes.push_back({909, "other", 3000, 0, {}});
        Situation a = Assess(s);
        CHECK(a.storyLive && a.snapVersion == kSnapVersion && a.storyReason.empty());
        CHECK(a.goals.size() == 3 && a.goals[0].source == "live" && a.goals[1].source == "live" && a.goals[2].source == "next");
        CHECK(!a.constraints.empty() && a.constraints[0].kind == Constraint::Kind::Leash && a.constraints[0].armed == 1);
        CHECK(a.diffBoth == 2 && a.diff.size() == 3);
        if (a.diff.size() == 3)
        {
            CHECK(a.diff[0].what == "goal" && a.diff[0].only == "live" && a.diff[0].name == "live_only");
            CHECK(a.diff[1].what == "goal" && a.diff[1].only == "ini" && a.diff[1].name == "gens_dead");
            CHECK(a.diff[2].what == "leash" && a.diff[2].only == "ini" && a.diff[2].name == "other");
        }
        const std::string d = Describe(a);
        CHECK(d.find("story source=live reason= both=2 only_live=1 only_ini=2\n") != std::string::npos);
        CHECK(d.find("diff goal only=ini name=gens_dead detail=SUCCEED\n") != std::string::npos);
        // round trip of the v2 records
        Snapshot back;
        std::string err;
        CHECK(Parse(Serialize(s), back, &err) && Serialize(back) == Serialize(s) && Assess(back).hash == a.hash);
        // a placeholder: the live table's script stands in
        Snapshot p = s;
        p.step = SnapStep();
        p.step.textId = 21660;
        p.objDefs.clear();
        p.marks.clear();
        p.goalsByTitle = p.goalsFromLast = true;
        CHECK(Assess(p).standIn == StandIn::Live && Assess(p).kindDetail.compare(0, 4, "live") == 0);
        p.goalsLive = false; // the fallback: the ini view's title stand-in, the reason printed
        p.liveOk = false, p.liveReason = "no story mission";
        a = Assess(p);
        CHECK(a.standIn == StandIn::Title && !a.storyLive && a.storyReason == "no story mission" &&
              a.goals[0].source == "ini" && a.constraints[0].armed == -1);
        CHECK(Describe(a).find("story source=ini reason=no%20story%20mission ") != std::string::npos);
    }
    {
        // the kind from the armed row that sets the next objective when no flying / destroy goal is armed (M13 docked)
        Snapshot s = SyntheticSnapshot();
        s.goalsLive = true;
        s.goals.clear();
        s.step.refCount = 0, s.step.refs[0] = s.step.refObj[0] = 0;
        s.step.verb = Verb::None;
        s.marks.clear();
        s.objDefs = {{"launch", "ids", "M13\\m13.ini"}};
        s.live = {{1, 0, "Cnd_SpaceEnter", "spawnships", "cnd_spaceenter", "x_obj", 2},
                  {2, 1, "Cnd_LocEnter", "inactive_one", "cnd_locenter", "y_obj", 0},
                  {3, 0, "", "exit_base", "cnd_baseexit", "defend_base", 0},
                  {4, 0, "Cnd_Destroyed", "dies", "cnd_destroyed", "FAIL", 0}};
        const Situation a = Assess(s);
        CHECK(objkinds::GoalTypeName(a.kind) == std::string("base_action") && a.kindSource == KindSource::Script &&
              a.kindDetail == "ids+live:cnd_baseexit");
        Snapshot ini = s; // the ini view never reads the rows' leads (v1 / the fallback)
        ini.goalsLive = false;
        CHECK(Assess(ini).kindSource == KindSource::Verb);
    }
    {
        // a v1 snapshot: parses, writes back byte for byte, prints no story line (its S1 hash kept)
        Snapshot s = SyntheticSnapshot();
        s.version = 1;
        for (SnapLive& x : s.live) // (v1 has no such fields: their defaults)
            x.leads.clear(), x.hops = -1;
        const std::string v1 = Serialize(s);
        CHECK(v1.compare(0, 8, "snap v1\n") == 0 && v1.find("source_live=") == std::string::npos &&
              v1.find(" hops=") == std::string::npos);
        Snapshot back;
        std::string err;
        CHECK(Parse(v1, back, &err) && back.version == 1 && Serialize(back) == v1);
        CHECK(Describe(Assess(back)).find("story ") == std::string::npos);
        // a v2 record in a v1 text is an error (fails closed)
        Snapshot w = s;
        w.iniGoals = w.goals;
        CHECK(!Parse(Serialize(w), back, &err));
    }

    // ---- the install's scripts over the recorded live rows (the fixtures' ids and states)
    if (!NeedInstall("the live goals over the install's scripts"))
        return;
    mission::UseScriptsFrom(g_test.install + "\\DATA\\MISSIONS");
    {
        mission::LiveTable lt;
        Snapshot snap;
        if (LiveOfFixture("m12_hole_s182.snap", lt, snap))
        {
            // SaveS182 (M12, "Awaiting" after the load, in Omicron Minor St01): the one armed flying goal is "within
            // 8 km of Toledo" (range_for_sendcomm_dx_m12_0500_vonclaussen -> dock_with_toledo_trigger -> land on the
            // base); the ini view (by title) had none
            g = mission::GoalsOfLive(lt, snap.system, snap.step.textId);
            CHECK(g.size() == 1);
            const mission::Goal* toledo = Find(g, "range_for_sendcomm_dx_m12_0500_vonclaussen");
            CHECK(toledo && toledo->kind == 1 && toledo->object == flhash::CreateId("St01_01") && toledo->dist == 8000 &&
                  toledo->leads == "mlog_land_on_the_base");
            CHECK(mission::ArmedLeashes(lt, snap.system).empty());
            if (snap.version >= 2) // (the S2 recording: the adapter took the same goal, source live)
            {
                CHECK(snap.goalsLive && snap.goals.size() == 1 && snap.goals[0].trigger == toledo->trigger &&
                      snap.iniGoals.empty());
                const Situation a = Assess(snap);
                CHECK(a.storyLive && a.standIn == StandIn::Live && !a.goals.empty() && a.goals[0].source == "live" &&
                      a.goals[0].trigger == "range_for_sendcomm_dx_m12_0500_vonclaussen");
            }
        }
        if (LiveOfFixture("m13_start_3989.snap", lt, snap))
        {
            // Save3989 (M13 docked at Toledo, "launch"): no armed flying goal; exit_base (Cnd_BaseExit) sets defend_base
            // at once - the ini view's two system-entry distance goals are not live
            CHECK(mission::GoalsOfLive(lt, snap.system, snap.step.textId).empty());
            const std::vector<mission::LiveLead> ll = mission::LeadsOfLive(lt, snap.step.textId);
            bool exitBase = false;
            for (size_t i = 0; i < ll.size(); ++i)
                if (lt.triggers[i].nickname == "exit_base")
                    exitBase = ll[i].leads == "defend_base" && ll[i].hops == 0;
            CHECK(exitBase);
            if (snap.version >= 2)
                CHECK(objkinds::GoalTypeName(Assess(snap).kind) == std::string("base_action"));
        }
        if (LiveOfFixture("m07_tau31_s208.snap", lt, snap))
        {
            // SaveS208 (M07 "Dock with the Trade Lane" at Holman's lane): tl_enter_to_holman (Cnd_TLEntered) is armed,
            // no flying goal; the ini view's by_entry goal at the Tau-23 lane is not live
            CHECK(mission::GoalsOfLive(lt, snap.system, snap.step.textId).empty());
            if (snap.version >= 2)
            {
                const Situation a = Assess(snap);
                CHECK(a.storyLive && !a.diff.empty() && a.diff[0].only == "ini" &&
                      a.diff[0].name == "tobias_at_active_tl_to_tau23_gate");
            }
        }
    }
    {
        // M13-MIMIC item 1: "Escort Sinclair to the Battleship Osiris" (24440, St01) ends when the PLAYER is within 1 km of
        // the Osiris (in_osiris_range); the death FAILs went off at trent_go_back (deact_general_triggers)
        mission::LiveTable et;
        et.ok = true;
        et.script = g_test.install + "\\DATA\\MISSIONS\\M13\\m13.ini";
        for (const char* n : {"in_osiris_range", "wait_for_trent_again", "sinclair_near_pika", "cleanup_orillion"})
            et.triggers.push_back(Row(n, 0));
        et.triggers.push_back(Row("sinclair_dies", 1));
        const u32 st01 = flhash::CreateId("St01");
        mission::EscortPlace e = mission::EscortByPlace(et, st01, 24440);
        CHECK(e.ok && e.goals.size() == 1 && e.goals[0].trigger == "in_osiris_range" && e.goals[0].kind == 1 &&
              e.goals[0].dist == 1000 && e.goals[0].leads == "dock_with_osiris");
        // Sinclair's death FAIL armed: refused
        et.triggers.back().state = 0;
        e = mission::EscortByPlace(et, st01, 24440);
        CHECK(!e.ok && e.why == "death-fail sinclair_dies");
        // no table / a placeholder: fail closed
        et.triggers.back().state = 1;
        CHECK(!mission::EscortByPlace(et, st01, 0).ok);
        mission::LiveTable none;
        CHECK(mission::EscortByPlace(none, st01, 24440).why == "no live table");
    }
    mission::UseScriptsFrom("");
}
