// LOCK-TRAP (core\game\mission.h LockTrapsOfLive, `lock_trap`, 31 Oct 2026): an armed "within R of <ship / point> ->
// Act_LockDock = Player, <dock>, lock" row is a trap only while that dock's unlock is still to come (an armed row other
// than the trap, or a trigger such a row arms within 4 Act_ActTrig hops, never through the trap, never into a complete
// row). A synthetic script with M13's shape (wait_for_trent beside in_hole_range -> check_cap), M12's (the unlock fired
// before the lock: lock_lair_when_player_exits), M11's (the unlock only through the trap's own chain: sye_Li01again), the
// reversed Act_LockDock form, the system filter and a table that is not ok; then the install's m13 / m12 / m11 rows.
#include "test.h"

#include "game/flhash.h"
#include "game/mission.h"
#include "game/mission_live.h"

#include <windows.h>

namespace
{
mission::LiveTrigger TrapRow(const char* nick, int state)
{
    mission::LiveTrigger r;
    r.id = flhash::CreateId(nick);
    r.state = state;
    r.nickname = nick;
    return r;
}

mission::LiveTable Table(const std::string& script, std::initializer_list<std::pair<const char*, int>> rows)
{
    mission::LiveTable t;
    t.ok = true;
    t.script = script;
    for (const auto& r : rows)
        t.triggers.push_back(TrapRow(r.first, r.second));
    return t;
}
} // namespace

void Test_mission_lock_trap()
{
    const std::string root = TempPath("", "LOCKT");
    const std::string path = TempPath("m95.ini", "LOCKT\\M95");
    CHECK(WriteTextFile(path, "[NNObjective]\nnickname = nn_follow\ntype = ids, 24455\n\n"
                              "[MsnShip]\nnickname = guide\nNPC = guide_npc\n\n"
                              "[NPC]\nnickname = guide_npc\nindividual_name = 21002\n\n"
                              "[MsnSolar]\nnickname = cap\n\n"
                              // M13's shape
                              "[Trigger]\nnickname = space_enter\nsystem = Li01\nCnd_SpaceEnter = Li01\n"
                              "Act_ActTrig = trap\nAct_ActTrig = near_gate\n\n"
                              "[Trigger]\nnickname = trap\nsystem = Li01\nCnd_DistShip = inside, Player, guide, 320\n"
                              "Act_LockDock = Player, Li01_to_Li02, lock\nAct_SetNNObj = nn_follow\n\n"
                              "[Trigger]\nnickname = near_gate\nsystem = Li01\nCnd_DistVec = inside, Player, 1000, 0, 2000, 2500\n"
                              "Act_ActTrig = kill_watch\n\n"
                              "[Trigger]\nnickname = kill_watch\nsystem = Li01\nCnd_Destroyed = cap, 1, ALL\n"
                              "Act_LockDock = Player, Li01_to_Li02, unlock\n\n"
                              // a point trap (Cnd_DistVec), its unlock armed
                              "[Trigger]\nnickname = point_trap\nsystem = Li01\nCnd_DistVec = inside, Player, 5, 0, 6, 400\n"
                              "Act_LockDock = Player, Li01_door, lock\n\n"
                              "[Trigger]\nnickname = door_unlock\nsystem = Li01\nCnd_CommComplete = dx_m95_0100\n"
                              "Act_LockDock = Player, Li01_door, unlock\n\n"
                              // M12's shape: the unlock fired before the lock
                              "[Trigger]\nnickname = exit_unlock\nsystem = Li01\nCnd_True = no_params\n"
                              "Act_LockDock = Player, Li01_exit, unlock\nAct_ActTrig = lock_after_exit\n\n"
                              "[Trigger]\nnickname = lock_after_exit\nCnd_DistVec = inside, Player, 850, 0, -1245, 400\n"
                              "Act_LockDock = Player, Li01_exit, lock\n\n"
                              // M11's shape: the unlock only through the trap's own chain
                              "[Trigger]\nnickname = self_trap\nsystem = Li01\nCnd_DistShip = inside, Player, Li01_hole, 1500\n"
                              "Act_LockDock = Player, Li01_hole2, lock\nAct_ActTrig = self_unlock\n\n"
                              "[Trigger]\nnickname = self_unlock\nCnd_SpaceExit = no_params\n"
                              "Act_LockDock = Player, Li01_hole2, unlock\n\n"
                              // M12 send_in_interceptors_at_3000's reversed form: not the player's lock
                              "[Trigger]\nnickname = rev_trap\nsystem = Li01\nCnd_DistVec = inside, Player, 0, 0, 0, 5000\n"
                              "Act_LockDock = Li01_rev, Player, lock\n\n"
                              "[Trigger]\nnickname = rev_unlock\nsystem = Li01\nCnd_CommComplete = dx_m95_0200\n"
                              "Act_LockDock = Player, Li01_rev, unlock\n\n"
                              // another system's
                              "[Trigger]\nnickname = li02_trap\nsystem = Li02\nCnd_DistShip = inside, Player, guide, 300\n"
                              "Act_LockDock = Player, Li02_gate, lock\n\n"
                              "[Trigger]\nnickname = li02_unlock\nsystem = Li02\nCnd_CommComplete = dx_m95_0300\n"
                              "Act_LockDock = Player, Li02_gate, unlock\n"));
    mission::UseScriptsFrom(root);
    const u32 li01 = flhash::CreateId("Li01");

    // ---- M13's shape before the gate: the trap armed, the unlock one hop past an armed row
    std::vector<mission::LockTrap> v =
        mission::LockTrapsOfLive(Table(path, {{"trap", 0}, {"near_gate", 0}, {"space_enter", 2}}), li01);
    CHECK(v.size() == 1);
    if (v.size() == 1)
    {
        const mission::LockTrap& t = v[0];
        CHECK(t.trigger == "trap" && t.kind == 1 && t.objectName == "guide" && t.object == flhash::CreateId("guide") &&
              t.nameIds == 21002 && t.dist == 320);
        CHECK(t.lockedName == "Li01_to_Li02" && t.locked == flhash::CreateId("Li01_to_Li02"));
        CHECK(t.unlockBy == "kill_watch" && t.unlockHops == 1);
    }
    // the unlock's row armed itself: hop 0
    v = mission::LockTrapsOfLive(Table(path, {{"trap", 0}, {"near_gate", 2}, {"kill_watch", 0}}), li01);
    CHECK(v.size() == 1 && v[0].unlockBy == "kill_watch" && v[0].unlockHops == 0);
    // the bug's state: the unlock fired (complete) - the lock would be final: no trap
    CHECK(mission::LockTrapsOfLive(Table(path, {{"trap", 0}, {"near_gate", 2}, {"kill_watch", 2}}), li01).empty());
    // the unlock's arming row inactive, nothing armed reaches it: no trap
    CHECK(mission::LockTrapsOfLive(Table(path, {{"trap", 0}, {"near_gate", 1}}), li01).empty());
    // the trap not armed (inactive / complete): none
    CHECK(mission::LockTrapsOfLive(Table(path, {{"trap", 1}, {"kill_watch", 0}}), li01).empty());
    CHECK(mission::LockTrapsOfLive(Table(path, {{"trap", 2}, {"kill_watch", 0}}), li01).empty());

    // ---- a point trap, in the table's order after the ship trap
    v = mission::LockTrapsOfLive(Table(path, {{"trap", 0}, {"kill_watch", 0}, {"point_trap", 0}, {"door_unlock", 0}}),
                                 li01);
    CHECK(v.size() == 2 && v[0].trigger == "trap");
    if (v.size() == 2)
        CHECK(v[1].trigger == "point_trap" && v[1].kind == 2 && v[1].pos[0] == 5 && v[1].pos[2] == 6 &&
              v[1].dist == 400 && v[1].lockedName == "Li01_door" && v[1].unlockBy == "door_unlock" &&
              v[1].unlockHops == 0);

    // ---- M12's shape (the unlock before the lock), M11's (only through the trap), the reversed form: none
    CHECK(mission::LockTrapsOfLive(Table(path, {{"lock_after_exit", 0}, {"exit_unlock", 2}}), li01).empty());
    CHECK(mission::LockTrapsOfLive(Table(path, {{"self_trap", 0}}), li01).empty());
    CHECK(mission::LockTrapsOfLive(Table(path, {{"rev_trap", 0}, {"rev_unlock", 0}}), li01).empty());

    // ---- the system filter
    const mission::LiveTable li02t = Table(path, {{"li02_trap", 0}, {"li02_unlock", 0}});
    CHECK(mission::LockTrapsOfLive(li02t, li01).empty());
    v = mission::LockTrapsOfLive(li02t, flhash::CreateId("Li02"));
    CHECK(v.size() == 1 && v[0].trigger == "li02_trap" && v[0].locked == flhash::CreateId("Li02_gate"));

    // ---- fails closed: a table that is not ok, ids no script knows
    mission::LiveTable bad = Table(path, {{"trap", 0}, {"kill_watch", 0}});
    bad.ok = false;
    CHECK(mission::LockTrapsOfLive(bad, li01).empty());
    mission::LiveTable foreign = Table("", {{"trap", 0}, {"kill_watch", 0}});
    for (mission::LiveTrigger& r : foreign.triggers)
        r.id ^= 0x5A5A5A5A;
    CHECK(mission::LockTrapsOfLive(foreign, li01).empty());
    mission::UseScriptsFrom("");
    DeleteFileA(path.c_str());

    // ---- the install's scripts
    if (!NeedInstall("the lock traps over the install's scripts"))
        return;
    mission::UseScriptsFrom(g_test.install + "\\DATA\\MISSIONS");
    const std::string missions = g_test.install + "\\DATA\\MISSIONS\\";
    {
        // M13 after the Osiris launch ("Meet Orillion", m13.ini 1808-1843 / 1921-1957): wait_for_trent (320 m of Orillion ->
        // lock St01_to_St03_hole) beside in_hole_range (armed at goto_hole_story) -> check_cap -> unlock
        const u32 st01 = flhash::CreateId("St01");
        const std::string m13 = missions + "M13\\m13.ini";
        v = mission::LockTrapsOfLive(Table(m13, {{"wait_for_trent", 0}, {"in_hole_range", 0}, {"lets_be_friends", 0}}),
                                     st01);
        CHECK(v.size() == 1);
        if (v.size() == 1)
            CHECK(v[0].trigger == "wait_for_trent" && v[0].kind == 1 && v[0].objectName == "Orillion" &&
                  v[0].nameIds == 217304 && v[0].dist == 320 && v[0].locked == flhash::CreateId("St01_to_St03_hole") &&
                  v[0].unlockBy == "check_cap" && v[0].unlockHops == 1);
        // check_cap fired first: the lock would be final - no trap (the ship must not meet him now)
        CHECK(mission::LockTrapsOfLive(Table(m13, {{"wait_for_trent", 0}, {"in_hole_range", 2}, {"check_cap", 2}}), st01)
                  .empty());
        // SaveS450's rows (M13.orillion, [TriggerSave]): wait_for_trent fired before the save - nothing to trap
        CHECK(mission::LockTrapsOfLive(Table(m13, {{"in_hole_range", 0}, {"near_nomad_gate", 0}, {"pause1", 0}}), st01)
                  .empty());
    }
    {
        // M12's lair exit: jumped_to_escape_2 unlocked it and armed lock_lair_when_player_exits - no unlock to come
        const std::string m12 = missions + "M12\\m12.ini";
        CHECK(mission::LockTrapsOfLive(Table(m12, {{"lock_lair_when_player_exits", 0}, {"jumped_to_escape_2", 2}}),
                                       flhash::CreateId("St02"))
                  .empty());
        // M11's sye_Li01again: its unlocks (bse_osiris2) come only through its own chain
        const std::string m11 = missions + "M11\\m11.ini";
        CHECK(mission::LockTrapsOfLive(Table(m11, {{"sye_Li01again", 0}, {"syx_Li01b", 0}, {"com_1461", 0}}),
                                       flhash::CreateId("Li01"))
                  .empty());
    }
    mission::UseScriptsFrom("");
}
