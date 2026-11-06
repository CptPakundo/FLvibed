// LOOK L0 (modules\autopilot\docs\LOOK_PLAN.md): the script readers it adds to core\game\mission.cpp - Goal kind 4 (the mission's
// ships destroyed, by label or nickname, only through a reader's `look` argument), the look extras (health gate,
// Act_Invulnerable, the chain's lane ride, the lead's order), ChainGoals, ShipIdsOf, the tethers - over a synthetic script;
// then the pure verdict (modules\autopilot\src\lookahead.h): kill an armed hostile live ship; none for a friendly /
// unspawned / untargetable / pre-hostile ship or a nag breach; the least-cost fly goal (the ring trigger when cheaper, the
// lane when faster); the leash and PSN blockers; the log key; then the install's m02.ini ("Follow Ashcroft" -> the kill
// candidate Ashcroft, his health gate and Act_Invulnerable), m05.ini (player_near_br04_01 and dst_to_br04_tradelane with
// its lane) and m04.ini's tethers. ASH-START (chase_kill / spawn_stage): FleeingSpawn, StagePoint, ChaseStep over synthetic
// facts; mission::SpawnAfterKill / LeavingNags and the stage point over m02.ini's escorts_destroyed -> Ashcroft.
#include "test.h"

#include "game/flhash.h"
#include "game/mission.h"
#include "game/mission_live.h"

#include "../modules/autopilot/src/lookahead.h"

#include <cstring>

namespace
{
const mission::Goal* FindG(const std::vector<mission::Goal>& gs, const char* trigger)
{
    for (const mission::Goal& g : gs)
        if (g.trigger == trigger)
            return &g;
    return nullptr;
}

void Set3(float* p, float x, float y, float z) { p[0] = x, p[1] = y, p[2] = z; }

// a hostile, live, targetable ship of a kill goal
lookahead::KillShip LiveHostile(const char* nick, u32 id, float x, float y, float z, float dist)
{
    lookahead::KillShip s;
    s.nick = nick, s.id = id;
    s.alive = s.hostile = s.targetable = true;
    Set3(s.pos, x, y, z);
    s.dist = dist;
    s.hull = 1;
    return s;
}

lookahead::FlyGoal Fly(const char* trigger, int order, float x, float y, float z, float r)
{
    lookahead::FlyGoal g;
    g.trigger = trigger, g.leads = std::string("nn_") + trigger, g.source = "chain";
    g.kind = 2, g.leadsOrder = order, g.placed = true, g.radius = r;
    Set3(g.pos, x, y, z);
    return g;
}

bool HasBlocker(const lookahead::LookResult& r, const std::string& b)
{
    for (const std::string& x : r.blockers)
        if (x.compare(0, b.size(), b) == 0)
            return true;
    return false;
}
} // namespace

void Test_look()
{
    using namespace lookahead;
    // ---------------------------------------------------------------- the script readers (synthetic m97.ini)
    const std::string root = TempPath("", "LOOK"); // (TempPath makes one folder level: the parent first)
    const std::string path = TempPath("m97.ini", "LOOK\\M97");
    CHECK(WriteTextFile(
        path, "[NNObjective]\nnickname = nn_follow\ntype = navmarker, Li01, 24610, 1, 0, 0, 5000\n\n"
              "[NNObjective]\nnickname = nn_tractor\ntype = ids, 24630\n\n"
              "[NNObjective]\nnickname = nn_lane\ntype = ids, 24640\n\n"
              "[NNObjective]\nnickname = nn_dock\ntype = ids, 24650\n\n"
              "[NNObjective]\nnickname = nn_x\ntype = ids, 24660\n\n"
              "[MsnShip]\nnickname = boss\nNPC = boss_npc\nlabel = thug\nlabel = boss\nposition = 1000, 0, 2000\n\n"
              "[MsnShip]\nnickname = grunt\nNPC = grunt_npc\nlabel = thug\n\n"
              "[MsnShip]\nnickname = juni\nNPC = juni_npc\n\n"
              "[MsnFormation]\nnickname = gang\nposition = 3000, 0, 4000\nship = grunt\n\n"
              "[NPC]\nnickname = boss_npc\nindividual_name = 21002\n\n"
              "[NPC]\nnickname = juni_npc\nindividual_name = 21003\n\n"
              "[Trigger]\nnickname = start\nsystem = Li01\nCnd_True = no_params\nAct_SetNNObj = nn_follow\n"
              "Act_ActTrig = boss_dead\nAct_ActTrig = boss_hp\nAct_ActTrig = thugs_dead\nAct_ActTrig = near_ring\n"
              "Act_ActTrig = comm1\nAct_SetVibe = boss, Player, REP_HOSTILE_MAXIMUM\n"
              "Act_NagDistTowards = POS, f_nag, King, 0, 0, 5000, 23198, 100\n"
              "Act_Invulnerable = boss, true, true, 0.5\n\n"
              "[Trigger]\nnickname = boss_dead\nsystem = Li01\nCnd_Destroyed = boss, 1, ALL\nAct_SetNNObj = nn_tractor\n\n"
              "[Trigger]\nnickname = boss_hp\nsystem = Li01\nCnd_HealthDec = boss, 0.35\nAct_ActTrig = kill_him\n\n"
              "[Trigger]\nnickname = kill_him\nsystem = Li01\nCnd_True = no_params\nAct_Destroy = boss\n"
              "Act_ActTrig = pod_timer\n\n"
              "[Trigger]\nnickname = pod_timer\nsystem = Li01\nCnd_Timer = 60\nAct_ChangeState = FAIL, 22360\n\n"
              "[Trigger]\nnickname = thugs_dead\nsystem = Li01\nCnd_Destroyed = thug, -1\nAct_ChangeState = SUCCEED\n\n"
              "[Trigger]\nnickname = near_ring\nsystem = Li01\nCnd_DistShip = inside, Player, Li01_ring, 4000\n"
              "Act_SetNNObj = mlog_awaiting_objective, OBJECTIVE_HISTORY\nAct_ActTrig = comm_a\n\n"
              "[Trigger]\nnickname = comm_a\nCnd_CommComplete = dx_a\nAct_SetNNObj = nn_dock\n\n"
              "[Trigger]\nnickname = comm1\nCnd_CommComplete = dx_1\nAct_ActTrig = to_lane\n\n"
              "[Trigger]\nnickname = to_lane\nCnd_DistVec = inside, Player, 0, 0, 10000, 3000\nAct_ActTrig = tl_enter\n"
              "Act_ActTrig = comm2\n\n"
              "[Trigger]\nnickname = comm2\nCnd_CommComplete = dx_2\nAct_SetNNObj = nn_lane\n\n"
              "[Trigger]\nnickname = tl_enter\nCnd_TLEntered = Player, ring_a, ring_b\nAct_SetNNObj = nn_x\n\n"
              "[Trigger]\nnickname = king_dies\nsystem = Li01\nCnd_Destroyed = juni, 1\nAct_ChangeState = FAIL, 1\n\n"
              "[Trigger]\nnickname = juni_tether\nsystem = Li01\nrepeatable = true\n"
              "Cnd_TetherBroke = juni, Player, 1500, 30, 2500\n\n"
              "[Trigger]\nnickname = hunt_30\nsystem = Li01\nCnd_TetherBroke = juni, Player, 3000, 30\n"
              "Act_ActTrig = hunt_60\n\n"
              "[Trigger]\nnickname = hunt_60\nsystem = Li01\nCnd_TetherBroke = juni, Player, 3000, 30\n"
              "Act_ChangeState = FAIL, 22750\n\n"
              "[Trigger]\nnickname = far_tether\nsystem = Li02\nCnd_TetherBroke = juni, Player, 900, 10\n"));
    mission::UseScriptsFrom(root);
    const u32 li01 = flhash::CreateId("Li01");
    {
        // without `look`: no kind 4 (every reader as before - the decision sweep's own check is decision_baseline)
        mission::AdvanceInfo info;
        const std::vector<mission::Goal> plain = mission::AdvanceGoalsPeek(24610, li01, 0, info, false);
        bool kind4 = false;
        for (const mission::Goal& g : plain)
            kind4 = kind4 || g.kind == 4;
        CHECK(!kind4 && !FindG(plain, "boss_dead") && FindG(plain, "near_ring"));
        // with `look`: kind 4 by nickname - the ship, its pilot, its spawn, the health gate, Act_Invulnerable
        const std::vector<mission::Goal> look = mission::AdvanceGoalsPeek(24610, li01, 0, info, true);
        const mission::Goal* bd = FindG(look, "boss_dead");
        CHECK(bd && bd->kind == 4 && bd->count == 1 && bd->ships.size() == 1 && bd->leads == "nn_tractor");
        if (bd && bd->ships.size() == 1)
        {
            CHECK(bd->ships[0].nick == "boss" && bd->ships[0].nameIds == 21002 && bd->ships[0].hasPos &&
                  bd->ships[0].pos[0] == 1000 && bd->ships[0].pos[2] == 2000);
            // the gate's immediate chain destroys him (the Cnd_Timer FAIL after it is the pod's deadline, not the gate's)
            CHECK(bd->healthGate == 0.35f && !bd->gateFails);
            CHECK(bd->invulnerable && bd->invSecond && bd->invFloor == 0.5f && bd->invTrigger == "start");
            CHECK(bd->objects.empty() && bd->leadsOrder == 1); // (nn_tractor: the second [NNObjective])
        }
        // kind 4 by label, count -1 = all: boss and grunt (grunt's spawn: its formation's)
        const mission::Goal* td = FindG(look, "thugs_dead");
        CHECK(td && td->kind == 4 && td->count == 2 && td->ships.size() == 2 && td->leads == "SUCCEED" &&
              td->leadsOrder == 5);
        if (td && td->ships.size() == 2)
            CHECK(td->ships[1].nick == "grunt" && td->ships[1].hasPos && td->ships[1].pos[0] == 3000);
        CHECK(!FindG(look, "king_dies")); // (a failing chain: no goal)
        // ShipIdsOf: by spawn position (2 m), only the goal's ships
        if (bd)
        {
            std::vector<mission::Spawn> spawns(2);
            spawns[0].id = 77, Set3(spawns[0].pos, 1000.5f, 0, 2000);
            spawns[1].id = 78, Set3(spawns[1].pos, 50, 0, 50);
            std::vector<std::string> names;
            const std::vector<u32> ids = mission::ShipIdsOf(*bd, spawns, &names);
            CHECK(ids.size() == 1 && ids[0] == 77 && names.size() == 1 && names[0] == "boss");
            mission::Goal notKill = *bd;
            notKill.kind = 1;
            CHECK(mission::ShipIdsOf(notKill, spawns).empty());
        }
        // the chain: now (hop 1) and about to be armed (after a comm line: hop 2), with the lane ride and the lead order
        const std::vector<mission::Goal> chain = mission::ChainGoals(24610, li01);
        const mission::Goal* nr = FindG(chain, "near_ring");
        const mission::Goal* tl = FindG(chain, "to_lane");
        CHECK(nr && nr->kind == 1 && nr->hops == 1 && nr->leads == "nn_dock" && nr->laneFrom.empty() &&
              nr->leadsOrder == 3);
        CHECK(tl && tl->kind == 2 && tl->hops == 2 && tl->leads == "nn_lane" && tl->laneFrom == "ring_a" &&
              tl->laneToward == "ring_b" && tl->leadsOrder == 2);
        CHECK(FindG(chain, "boss_dead") && FindG(chain, "boss_dead")->hops == 1);
        CHECK(!FindG(chain, "tl_enter"));                     // (a lane condition: no goal)
        CHECK(mission::ChainGoals(21660, li01).empty());       // (a text no script knows: no step)
        // the tethers: every row with its fields, the failing chain marked, the system's only
        const std::vector<mission::Tether> te = mission::Tethers(path, li01);
        CHECK(te.size() == 3);
        if (te.size() == 3)
        {
            CHECK(te[0].trigger == "juni_tether" && te[0].a == "juni" && te[0].b == "Player" && te[0].dist == 1500 &&
                  te[0].secs == 30 && te[0].inner == 2500 && !te[0].fails && te[0].aNameIds == 21003);
            CHECK(te[1].trigger == "hunt_30" && te[1].inner == 0 && te[1].fails);
            CHECK(te[2].trigger == "hunt_60" && te[2].fails);
        }
        CHECK(mission::Tethers(path, flhash::CreateId("Li02")).size() == 1);
        // (the leashes are untouched: a tether is no leash)
        mission::AdvanceGoals(24610, li01);
        CHECK(mission::Leashes(li01).empty() && mission::Tethers("", li01).size() == 3);
    }
    mission::UseScriptsFrom("");

    // ---------------------------------------------------------------- the verdict (synthetic facts)
    LookFacts base;
    base.havePilot = true;
    Set3(base.me, 0, 0, 0);
    base.flySpeed = 300;
    base.textId = 24610;
    {
        // an armed hostile live ship -> kill
        LookFacts f = base;
        KillGoal k;
        k.trigger = "boss_dead", k.leads = "nn_tractor", k.source = "armed", k.count = 1;
        k.ships.push_back(LiveHostile("boss", 7, 0, 0, 1500, 1500));
        f.kills.push_back(k);
        LookResult r = LookTarget(f);
        CHECK(r.kind == Kind::Kill && r.target == 7 && r.ship == "boss" && r.trigger == "boss_dead" && r.blockers.empty());
        CHECK(Line(r, f).compare(0, 12, "kill boss=7 ") == 0);
        // the nearest of two
        f.kills[0].ships.push_back(LiveHostile("grunt", 8, 0, 0, 900, 900));
        CHECK(LookTarget(f).target == 8);
        f.kills[0].ships.pop_back();
        // friendly / unspawned / untargetable (cloak stand-in) / pre-hostile / not hostile / dead / a failing gate -> none
        const auto none = [&](void (*edit)(LookFacts&), const char* code)
        {
            LookFacts g = f;
            edit(g);
            const LookResult x = LookTarget(g);
            CHECK(x.kind == Kind::None && HasBlocker(x, std::string("boss_dead/boss:") + code));
        };
        none([](LookFacts& g) { g.kills[0].ships[0].friendly = true, g.kills[0].ships[0].hostile = false; }, "friendly");
        none([](LookFacts& g) { g.kills[0].ships[0].id = 0; }, "unspawned");
        none([](LookFacts& g) { g.kills[0].ships[0].targetable = false; }, "untargetable");
        none([](LookFacts& g) { g.kills[0].ships[0].preHostile = true, g.kills[0].ships[0].hostile = false; },
             "pre-hostile");
        none([](LookFacts& g) { g.kills[0].ships[0].hostile = false; }, "not-hostile");
        none([](LookFacts& g) { g.kills[0].ships[0].alive = false; }, "dead");
        none([](LookFacts& g) { g.kills[0].gateFails = true; }, "gate-fails");
        // the towards nag: a ship farther from the nag's target than we are -> breach; one ahead of us toward it -> kill
        LookFacts n = f;
        n.nag.has = n.nag.placed = true, n.nag.name = "f_nag", n.nag.radius = 100;
        Set3(n.nag.pos, 0, 0, -5000); // behind us; the ship is ahead
        r = LookTarget(n);
        CHECK(r.kind == Kind::None && HasBlocker(r, "boss_dead/boss:nag(f_nag)"));
        Set3(n.nag.pos, 0, 0, 5000); // the ship lies toward it
        CHECK(LookTarget(n).kind == Kind::Kill);
        // a leash: the ship beyond 85 % of it from its anchor
        LookFacts l = f;
        LeashAnchor a;
        a.name = "juni", a.found = true, a.dist = 1000;
        Set3(a.pos, 0, 0, 0);
        l.leashes.push_back(a);
        r = LookTarget(l);
        CHECK(r.kind == Kind::None && HasBlocker(r, "boss_dead/boss:leash(juni)"));
        // the key: distances do not change it, a blocker does
        LookFacts moved = f;
        moved.kills[0].ships[0].dist = 1400, moved.kills[0].ships[0].pos[2] = 1400;
        CHECK(LookTarget(moved).key == LookTarget(f).key);
        CHECK(LookTarget(l).key != LookTarget(f).key);
        // the invincible line: the script's flags and floor against the game's
        LookFacts iv = f;
        iv.kills[0].scriptInv = iv.kills[0].scriptInvSecond = true, iv.kills[0].scriptInvFloor = 0.5f;
        iv.kills[0].invTrigger = "start", iv.kills[0].healthGate = 0.35f;
        iv.kills[0].ships[0].invRead = iv.kills[0].ships[0].inv = true, iv.kills[0].ships[0].invFloor = 0.5f;
        iv.kills[0].ships[0].hull = 0.62f;
        const std::vector<std::string> il = InvincibleLines(iv);
        CHECK(il.size() == 1 && il[0] == "invincible boss=7 trigger=boss_dead script=start:true,1,0.50 game=1,0,0.500 "
                                         "hull=0.620 gate=0.35");
        CHECK(InvincibleLines(f).empty());
    }
    {
        // M05-style: the ring trigger (advances further: order 5) far off by direct flight vs the lane trigger (order 4)
        // near us whose lane ride ends beside the ring
        LookFacts f = base;
        FlyGoal ring = Fly("player_near", 5, 0, 0, 40000, 4000);
        FlyGoal lane = Fly("dst_to_lane", 4, 0, 0, 3500, 3000);
        lane.laneFrom = "ring_3", lane.lane = true, lane.laneEnd[2] = 38000;
        // (a) a slow lane (300 s ride): the ring trigger is cheaper -> fly to it
        lane.laneRideS = 300;
        f.flies = {ring, lane};
        LookResult r = LookTarget(f);
        CHECK(r.kind == Kind::Fly && r.trigger == "player_near" && r.nearest == "dst_to_lane");
        CHECK_NEAR(r.cost, 36000.0 / 300, 0.01);
        CHECK_NEAR(PathCost(f.flies[1], f, &f.flies[0]), 500.0 / 300 + 300, 0.01);
        // (b) a fast lane (20 s ride): the lane keeps it -> fly to the lane trigger
        f.flies[1].laneRideS = 20;
        r = LookTarget(f);
        CHECK(r.kind == Kind::Fly && r.trigger == "dst_to_lane");
        CHECK_NEAR(r.cost, 500.0 / 300 + 20, 0.01);
        CHECK(r.point[2] > 0 && r.point[2] < 3500); // (the near side, on our side)
        // inside a goal already: not a trip (it fires on its own)
        LookFacts in = f;
        Set3(in.me, 0, 0, 3000);
        CHECK(HasBlocker(LookTarget(in), "dst_to_lane:inside"));
        // unplaced (a solar the game does not place): refused
        LookFacts up = f;
        up.flies[0].placed = false;
        CHECK(HasBlocker(LookTarget(up), "player_near:unplaced"));
        // a leash the ring trip would break: refused, the lane goal stays
        LookFacts l = f;
        LeashAnchor a;
        a.name = "juni", a.found = true, a.dist = 6000;
        l.leashes.push_back(a);
        r = LookTarget(l);
        CHECK(HasBlocker(r, "player_near:leash(juni)") && r.trigger == "dst_to_lane");
        // PSN: an unspawned kill ship's spawn beside the lane goal -> refused there; the declared next destroy place
        LookFacts p = f;
        KillGoal k;
        k.trigger = "cap_dead";
        KillShip cap;
        cap.nick = "cap2", cap.spawnKnown = true;
        Set3(cap.spawn, 0, 0, 2000);
        k.ships.push_back(cap);
        p.kills.push_back(k);
        r = LookTarget(p);
        CHECK(r.kind == Kind::Fly && r.trigger == "player_near" && HasBlocker(r, "dst_to_lane:psn(cap2 spawn") &&
              HasBlocker(r, "cap_dead/cap2:unspawned"));
        float q[3] = {0, 0, 2500};
        std::string why;
        CHECK(!StageablePlace(q, p, &why) && why.find("cap2") == 0);
        q[2] = 9000;
        CHECK(StageablePlace(q, p, &why));
        LookFacts nx = f;
        nx.nextPlace = nx.nextDestroy = true, nx.nextText = 25000;
        Set3(nx.nextPos, 0, 0, 39000);
        r = LookTarget(nx);
        CHECK(!r.nextOk && r.nextWhy.find("destroy objective 25000") == 0 && HasBlocker(r, "player_near:psn("));
        nx.nextDestroy = false;
        CHECK(LookTarget(nx).nextOk);
        // a kill outranks a fly
        LookFacts both = f;
        KillGoal kb;
        kb.trigger = "boss_dead";
        kb.ships.push_back(LiveHostile("boss", 9, 0, 0, 2000, 2000));
        both.kills.push_back(kb);
        CHECK(LookTarget(both).kind == Kind::Kill);
        // no pilot: no fly verdict
        LookFacts nop = f;
        nop.havePilot = false;
        CHECK(LookTarget(nop).kind == Kind::None);
    }

    // ---------------------------------------------------------------- TIMER-DESTROY's verdict (pure)
    {
        // the look0 M13 death's place: 3 km from NomadCap2 (6350, 0, -5080), Toledo's ring 1658, 0, -2026, 0 batteries
        TimerDestroyFacts t;
        t.advance = "timer", t.speedrun = true, t.havePilot = true, t.hull = 0.83f, t.batteries = 0;
        Set3(t.me, 5500, 0, -3977);
        t.nextPlace = true, Set3(t.nextPos, 1658, 0, -2026);
        t.stepPlace = true, Set3(t.stepPos, 6350, 0, -5080);
        TdResult r = TimerDestroy(t);
        CHECK(r.verdict == TdVerdict::Stage && r.why == "next");
        CHECK_NEAR(r.dist, 4308, 2); // (3842, 1951)
        // the point: 300 m short of the ring on our side
        const float pd[3] = {r.point[0] - 1658, r.point[1], r.point[2] + 2026};
        CHECK_NEAR(std::sqrt(pd[0] * pd[0] + pd[1] * pd[1] + pd[2] * pd[2]), kTdStandOff, 1);
        CHECK(r.point[0] > 1658 && r.point[2] < -2026);
        // attacked on the way, far from the place: the trip goes on; at the place: defend
        t.attacked = true;
        CHECK(TimerDestroy(t).verdict == TdVerdict::Stage);
        TimerDestroyFacts at = t;
        Set3(at.me, 1900, 0, -2150);
        CHECK(TimerDestroy(at).verdict == TdVerdict::Defend);
        at.attacked = false;
        r = TimerDestroy(at);
        CHECK(r.verdict == TdVerdict::Rest && r.why == "at-place");
        // PSN: a next place that is a destroy, or within 3 km of the step's own target -> no staging
        TimerDestroyFacts ps = t;
        ps.attacked = false, ps.nextDestroy = true;
        r = TimerDestroy(ps);
        CHECK(r.verdict == TdVerdict::Rest && r.why == "psn(destroy)");
        ps.nextDestroy = false, Set3(ps.nextPos, 6000, 0, -4000);
        r = TimerDestroy(ps);
        CHECK(r.verdict == TdVerdict::Rest && r.why == "psn");
        ps.nextPlace = false;
        CHECK(TimerDestroy(ps).why == "no-place");
        // the default preset: fights while safe (hull >= 0.6, batteries >= 7), else rests / defends - no staging
        TimerDestroyFacts d = t;
        d.speedrun = false, d.attacked = false, d.batteries = 12, d.hull = 0.9f;
        CHECK(TimerDestroy(d).verdict == TdVerdict::Fight);
        d.batteries = 3;
        r = TimerDestroy(d);
        CHECK(r.verdict == TdVerdict::Rest && r.why == "unsafe(batteries)");
        d.batteries = 12, d.hull = 0.4f, d.attacked = true;
        r = TimerDestroy(d);
        CHECK(r.verdict == TdVerdict::Defend && r.why == "unsafe(hull)");
        d.batteries = -1, d.hull = 0.9f, d.attacked = false; // unknown is not safe
        CHECK(TimerDestroy(d).verdict == TdVerdict::Rest);
        // a kill-advanced step (or unknown): today's decision
        TimerDestroyFacts k = t;
        k.advance = "kill";
        CHECK(TimerDestroy(k).verdict == TdVerdict::Today);
        k.advance = "";
        CHECK(TimerDestroy(k).verdict == TdVerdict::Today);
        CHECK(strcmp(TdVerdictName(TdVerdict::Stage), "stage") == 0);
        // td_window: the script takes the dock now -> dock (speedrun only; never a destroy objective's object)
        TimerDestroyFacts dn = t;
        dn.attacked = false, dn.dockNow = true, dn.nextObject = 0x1234;
        r = TimerDestroy(dn);
        CHECK(r.verdict == TdVerdict::Dock && r.why == "unlocked");
        dn.nextObject = 0;
        CHECK(TimerDestroy(dn).verdict == TdVerdict::Stage);
        dn.nextObject = 0x1234, dn.speedrun = false, dn.hull = 0.9f, dn.batteries = 0;
        CHECK(TimerDestroy(dn).verdict == TdVerdict::Rest);
    }

    // ---------------------------------------------------------------- SPAWN-STAGE / CHASE (pure)
    {
        // a fleeing spawn: a goto 25 km away, nothing else near
        SpawnFacts sf;
        sf.hasPos = true, Set3(sf.spawn, 0, 0, 0);
        sf.ordersFound = true;
        sf.cmds = {{"setpriority", "NORMAL"}, {"gotovec", "goto_cruise, 0, 0, 25000, 200, true"},
                   {"breakformation", "no_params"}};
        SpawnFacts::Other far;
        far.nick = "thug1", far.hasPos = true, Set3(far.pos, 0, 0, 25000);
        sf.others.push_back(far);
        float run[3] = {};
        std::string why;
        CHECK(FleeingSpawn(sf, run, &why) && run[2] == 25000 && why == "flees 25000 m");
        // refusals: a hostile spawn beside him, unplaced; orders at the player first / anywhere; a delay first; no goto;
        // a short run; no orders; no position
        SpawnFacts h = sf;
        Set3(h.others[0].pos, 0, 0, 2000);
        CHECK(!FleeingSpawn(h, run, &why) && why == "thug1 spawns 2000 m off");
        h.others[0].friendly = true; // (a friend beside him is no danger)
        CHECK(FleeingSpawn(h, run, &why));
        h = sf, h.others[0].hasPos = false;
        CHECK(!FleeingSpawn(h, run, &why) && why == "thug1 unplaced");
        using Cmd = std::pair<std::string, std::string>;
        h = sf, h.cmds.insert(h.cmds.begin(), Cmd("gotoship", "goto, Player, 200, true"));
        CHECK(!FleeingSpawn(h, run, &why) && why == "at-player(gotoship)");
        h = sf, h.cmds.push_back(Cmd("follow", "Player, 2500, -100, 50, -200"));
        CHECK(!FleeingSpawn(h, run, &why) && why == "at-player(follow)");
        h = sf, h.cmds = {{"followplayer", "no_params"}};
        CHECK(!FleeingSpawn(h, run, &why) && why == "at-player(followplayer)");
        h = sf, h.cmds.insert(h.cmds.begin(), Cmd("delay", "10"));
        CHECK(!FleeingSpawn(h, run, &why) && why == "delay first");
        h = sf, h.cmds = {{"setpriority", "ALWAYS_EXECUTE"}};
        CHECK(!FleeingSpawn(h, run, &why) && why == "no-goto");
        h = sf, h.cmds[1].second = "goto_cruise, 0, 0, 2500, 200";
        CHECK(!FleeingSpawn(h, run, &why) && why == "short-run 2500 m");
        h = sf, h.ordersFound = false;
        CHECK(!FleeingSpawn(h, run, &why) && why == "no-orders");
        h = sf, h.hasPos = false;
        CHECK(!FleeingSpawn(h, run, &why) && why == "no-spawn-pos");
        // a spline: its last point
        h = sf, h.cmds[1] = {"gotospline", "goto_cruise, 0, 0, 100, 0, 0, 200, 0, 0, 300, 9000, 0, 9000, 200"};
        CHECK(FleeingSpawn(h, run, &why) && run[0] == 9000 && run[2] == 9000);

        // the stage point: 200 m behind the spawn on his line, 250 m beside it on the anchor's side; pulled into 80 % of
        // the leash
        const float spawn[3] = {0, 0, 0}, to[3] = {0, 0, 10000};
        StageResult s = StagePoint(spawn, to, nullptr, 0);
        CHECK(s.ok && s.why == "free" && !s.clamped && s.dAnchor < 0);
        CHECK_NEAR(s.p[2], -200, 0.01);
        CHECK_NEAR(std::fabs(s.p[0]), 250, 0.01);
        CHECK_NEAR(s.u[2], 1, 1e-4);
        const float left[3] = {-3000, 0, 0}, right[3] = {3000, 0, 0};
        CHECK(StagePoint(spawn, to, left, 0).p[0] < 0 && StagePoint(spawn, to, right, 0).p[0] > 0);
        const float anchorFar[3] = {-3000, 0, -4000};
        s = StagePoint(spawn, to, anchorFar, 5000); // (|(-250, -200) - (-3000, -4000)| = 4690 > 4000: pulled)
        CHECK(s.ok && s.clamped && s.why == "clamped");
        CHECK_NEAR(s.dAnchor, 4000, 0.5);
        CHECK(s.p[0] < -250 && s.p[2] < -200); // (toward the anchor)
        const float anchorNear[3] = {-500, 0, 0};
        s = StagePoint(spawn, to, anchorNear, 5000);
        CHECK(s.ok && !s.clamped && s.why == "free");
        CHECK(!StagePoint(spawn, spawn, nullptr, 0).ok);

        // the chase's step: a slow target is a plain fight; a fast one is charged; in cruise at 110-250 m ahead: coast;
        // the coast ends on the gap, the shield or his death
        ChaseFacts c;
        c.gap = 1500, c.bearingDeg = 5, c.mySpeed = 80, c.hisSpeed = 75, c.engineTop = 80, c.gunRange = 600;
        c.shield = 1;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Fight);
        c.hisSpeed = 290;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge);
        c.cruising = true, c.mySpeed = 295, c.gap = 200;
        ChaseResult cr = ChaseStep(c);
        CHECK(cr.verdict == ChaseVerdict::Coast && strcmp(cr.why, "start") == 0);
        c.bearingDeg = 45;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (not ahead: on with the cruise)
        c.bearingDeg = 5, c.gap = 90;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge);
        c.gap = 200, c.mySpeed = 120;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (still charging: no coast yet)
        c.mySpeed = 295, c.shield = 0.1f;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Charge); // (no coast on an empty shield)
        c.shield = 1, c.coasting = true, c.cruising = false;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Coast);
        c.gap = 541;
        cr = ChaseStep(c);
        CHECK(cr.verdict == ChaseVerdict::EngineOn && strcmp(cr.why, "gap") == 0);
        c.gap = 200, c.shield = 0.1f;
        cr = ChaseStep(c);
        CHECK(cr.verdict == ChaseVerdict::EngineOn && strcmp(cr.why, "shield") == 0);
        c.shield = 1, c.killed = true;
        cr = ChaseStep(c);
        CHECK(cr.verdict == ChaseVerdict::EngineOn && strcmp(cr.why, "gone") == 0);
        c.coasting = false;
        CHECK(ChaseStep(c).verdict == ChaseVerdict::Fight);
        CHECK(strcmp(ChaseVerdictName(ChaseVerdict::EngineOn), "engine-on") == 0);
    }

    // ---------------------------------------------------------------- ALLY-KILL's verdict (pure)
    {
        // M13's gate: the hole at 46983, 0, -10694 (st01.ini), the ship ~2.5 km out at in_hole_range, NomadCap3 alive
        AllyKillFacts a;
        a.qualifies = a.speedrun = a.havePilot = true;
        Set3(a.me, 46983, 0, -8194);
        a.dockKnown = true, Set3(a.dockPos, 46983, 0, -10694);
        a.target = 0x5150, a.targetAlive = true, a.targetHull = 1, a.hull = 0.9f, a.batteries = 20;
        TdResult r = AllyKill(a);
        // far: a point trip kAkStandOff short of the hole on our side
        CHECK(r.verdict == TdVerdict::Stage && r.why == "to-dock");
        CHECK_NEAR(r.dist, 2500, 1);
        CHECK_NEAR(r.point[2], -10694 + kAkStandOff, 1);
        CHECK_NEAR(r.point[0], 46983, 1);
        // attacked on the trip: beyond kAkDefendReach the trip goes on, within it defend
        a.attacked = true;
        CHECK(AllyKill(a).verdict == TdVerdict::Stage);
        AllyKillFacts n = a;
        Set3(n.me, 46983, 0, -9694); // 1 km out
        r = AllyKill(n);
        CHECK(r.verdict == TdVerdict::Defend && r.why == "attacked(stage)");
        // at the dock: rest, or defend when attacked
        AllyKillFacts d = a;
        Set3(d.me, 47100, 0, -10600); // ~150 m
        r = AllyKill(d);
        CHECK(r.verdict == TdVerdict::Defend && r.why == "attacked(dock)");
        d.attacked = false;
        r = AllyKill(d);
        CHECK(r.verdict == TdVerdict::Rest && r.why == "waiting");
        d.allies = 3;
        CHECK(AllyKill(d).why == "allies");
        // the fallback: >= 20 s at the dock, no ally on it, < 0.05 hull lost -> the fight (fails closed)
        AllyKillFacts fb = d;
        fb.allies = 0, fb.waitedS = 25, fb.hullDrop = 0.01f;
        r = AllyKill(fb);
        CHECK(r.verdict == TdVerdict::Fight && r.why == "fallback");
        fb.hullDrop = 0.2f; // it is losing hull: the wait goes on
        CHECK(AllyKill(fb).verdict == TdVerdict::Rest);
        fb.hullDrop = 0.01f, fb.allies = 1; // a friendly on it: the wait goes on
        CHECK(AllyKill(fb).verdict == TdVerdict::Rest);
        fb.allies = 0, fb.waitedS = 19;
        CHECK(AllyKill(fb).verdict == TdVerdict::Rest);
        // (probe ak1: allies "on" it but no hull lost for 30 s at the dock -> the fight too; losing hull -> the wait goes on)
        fb.allies = 8, fb.waitedS = 30, fb.hullDrop = 0.0f;
        r = AllyKill(fb);
        CHECK(r.verdict == TdVerdict::Fight && r.why == "fallback(allies-stall)");
        fb.hullDrop = 0.10f;
        CHECK(AllyKill(fb).verdict == TdVerdict::Rest);
        fb.hullDrop = 0.0f, fb.waitedS = 29;
        CHECK(AllyKill(fb).verdict == TdVerdict::Rest);
        // (the fallback away from the dock too, once it waited; and kept for the step once fired)
        AllyKillFacts fa = a;
        fa.attacked = false, fa.waitedS = 30;
        CHECK(AllyKill(fa).verdict == TdVerdict::Fight);
        AllyKillFacts kept = d;
        kept.allies = 4, kept.fellBack = true;
        r = AllyKill(kept);
        CHECK(r.verdict == TdVerdict::Fight && r.why == "fallback(kept)");
        // today's decision: not the shape, no knowledge (exposed), no pilot, the target dead / unseen, no dock place
        AllyKillFacts t = d;
        t.qualifies = false;
        r = AllyKill(t);
        CHECK(r.verdict == TdVerdict::Today && r.why == "no-rule");
        t = d, t.speedrun = false;
        r = AllyKill(t);
        CHECK(r.verdict == TdVerdict::Today && r.why == "exposed");
        t = d, t.havePilot = false;
        CHECK(AllyKill(t).verdict == TdVerdict::Today);
        t = d, t.targetAlive = false;
        r = AllyKill(t);
        CHECK(r.verdict == TdVerdict::Today && r.why == "target-gone");
        t = d, t.target = 0;
        CHECK(AllyKill(t).verdict == TdVerdict::Today);
        t = d, t.dockKnown = false;
        r = AllyKill(t);
        CHECK(r.verdict == TdVerdict::Today && r.why == "no-dock");
        // (the default facts: today)
        CHECK(AllyKill(AllyKillFacts()).verdict == TdVerdict::Today);
    }

    // ---------------------------------------------------------------- the install's scripts
    if (!NeedInstall("LOOK over the install's m02 / m05 / m04"))
        return;
    mission::UseScriptsFrom(g_test.install + "\\DATA\\MISSIONS");
    {
        // M02 "Follow Ashcroft" (hunt2, 22262, Li03): start_stage4_attack sets it and arms ashcroft_dead (Cnd_Destroyed =
        // Ashcroft, 1, ALL -> tractor), ashcroft_health_35 (-> destroy_him: Act_Destroy) and sets Act_Invulnerable =
        // Ashcroft, true, true, 0.5
        const u32 li03 = flhash::CreateId("Li03");
        mission::AdvanceInfo info;
        const std::vector<mission::Goal> armed = mission::AdvanceGoalsPeek(22262, li03, 0, info, true);
        const mission::Goal* ad = FindG(armed, "ashcroft_dead");
        CHECK(ad && ad->kind == 4 && ad->count == 1 && ad->leads == "tractor" && ad->ships.size() == 1);
        if (ad && ad->ships.size() == 1)
        {
            CHECK(ad->ships[0].nick == "ashcroft" && ad->ships[0].hasPos && ad->ships[0].pos[0] == -28500 &&
                  ad->ships[0].nameIds != 0);
            CHECK(ad->healthGate == 0.35f && !ad->gateFails);
            CHECK(ad->invulnerable && ad->invSecond && ad->invFloor == 0.5f && ad->invTrigger == "start_stage4_attack");
            // the verdict with Ashcroft live and hostile in the contact list: kill him now
            LookFacts f = base;
            f.textId = 22262;
            KillGoal k = KillGoalOf(*ad, "armed");
            k.ships[0].id = 501, k.ships[0].alive = k.ships[0].hostile = k.ships[0].targetable = true;
            Set3(k.ships[0].pos, 0, 0, 2500), k.ships[0].dist = 2500;
            f.kills.push_back(k);
            const LookResult r = LookTarget(f);
            CHECK(r.kind == Kind::Kill && r.ship == "ashcroft" && r.target == 501 && r.leads == "tractor");
            CHECK(InvincibleLines(f).size() == 1);
        }
        CHECK(FindG(mission::ChainGoals(22262, li03), "ashcroft_dead"));
        mission::TowardsNag nag;
        CHECK(mission::TowardsNagOf(22262, nag) && nag.pos && nag.dist == 100 && nag.p[0] == -22908);

        // SPAWN-STAGE: "Destroy the Enemy Rogues" (destroy_rogues, 22269) - escorts_destroyed (Cnd_Destroyed Escort, 4) is
        // the step's armed kill goal; its firing spawns Ashcroft at -28500, 0, 51100 (m02.ini 273-277, 1541-1547), whose
        // first orders (start_run1: ashcroft_stage2) are a cruise goto to his run point -22099, 0, 75896; Thug1..4 spawn
        // there, 25 km off; ashcroft_dead (armed by start_stage4_attack) names him
        const std::vector<mission::Goal> rogues = mission::AdvanceGoalsPeek(22269, li03, 0, info, true);
        const mission::Goal* ed = FindG(rogues, "escorts_destroyed");
        CHECK(ed && ed->kind == 4 && ed->count == 4 && ed->ships.size() == 4);
        const std::string m02 = g_test.install + "\\DATA\\MISSIONS\\M02\\m02.ini";
        const mission::SpawnAfter sa = mission::SpawnAfterKill("escorts_destroyed", ed ? ed->file : m02);
        if (!ed || !sa.ok)
        {
            printf("spawn-stage m02 debug: why=%s file=%s goals=%u\n", sa.why.c_str(), ed ? ed->file.c_str() : m02.c_str(),
                   static_cast<unsigned>(rogues.size()));
            for (const mission::Goal& g : rogues)
                printf("  goal %s kind=%d count=%d ships=%u leads=%s\n", g.trigger.c_str(), g.kind, g.count,
                       static_cast<unsigned>(g.ships.size()), g.leads.c_str());
        }
        CHECK(sa.ok && sa.ship == "ashcroft" && sa.killTrigger == "ashcroft_dead" && sa.hasPos &&
              sa.pos[0] == -28500 && sa.pos[2] == 51100);
        CHECK(sa.orders == "ashcroft_stage2" && sa.ordersFound && !sa.cmds.empty() && sa.cmds[0].first == "gotovec");
        int thugs = 0;
        for (const mission::SpawnAfter::Other& o : sa.others)
            thugs += o.nick.compare(0, 4, "thug") == 0 && o.hasPos && o.pos[0] == -22099 && !o.friendly ? 1 : 0;
        CHECK(thugs == 4 && sa.others.size() == 4);
        SpawnFacts sf = SpawnFactsOf(sa);
        float run[3] = {};
        std::string why;
        CHECK(FleeingSpawn(sf, run, &why) && run[0] == -22099 && run[2] == 75896);
        const std::string flees = why;
        // ...refused when a hostile spawns beside him, or his first order is an attack on the player
        SpawnFacts near = sf;
        if (!near.others.empty())
        {
            near.others[0].pos[0] = -28000, near.others[0].pos[2] = 52000;
            CHECK(!FleeingSpawn(near, run, &why) && why.find("thug1 spawns") == 0);
        }
        SpawnFacts att = sf;
        att.cmds.insert(att.cmds.begin(), std::pair<std::string, std::string>("gotoship", "goto, Player, 100, true"));
        CHECK(!FleeingSpawn(att, run, &why) && why == "at-player(gotoship)");
        CHECK(!mission::SpawnAfterKill("ashcroft_dead", m02).ok); // (his own kill spawns no later kill ship)
        CHECK(mission::SpawnAfterKill("no_such_trigger", m02).why == "no trigger");
        // the fight's leaving nag: nag_battle3 (King, 5000 m, 13093), set by convoy_caught, off by escorts_destroyed
        const mission::LeavingNag* nb = nullptr;
        const std::vector<mission::LeavingNag> nags = mission::LeavingNags(m02, li03);
        for (const mission::LeavingNag& n : nags)
            if (n.name == "nag_battle3")
                nb = &n;
        CHECK(nb && nb->trigger == "convoy_caught" && nb->object == "King" && nb->dist == 5000 && nb->failIds == 13093 &&
              nb->nameIds == 216201 && !nb->pos);
        if (nb)
            CHECK(std::find(nb->offBy.begin(), nb->offBy.end(), "escorts_destroyed") != nb->offBy.end());
        // the stage point with King where our 4th escort died (FLMotion, M02.md: -29951, 46688 - 4.6 km from the spawn):
        // pulled into 4 km of him, ~0.7 km from the spawn
        const float king[3] = {-29951, 0, 46688};
        const StageResult st = StagePoint(sa.pos, run, king, nb ? nb->dist : 5000);
        CHECK(st.ok && st.clamped && st.dAnchor <= 4000.5f && st.dSpawn < 1000);
        if (g_test.verbose)
            printf("spawn-stage m02: P=%.0f,%.0f d_spawn=%.0f d_king=%.0f %s; flee: %s\n", st.p[0], st.p[2], st.dSpawn,
                   st.dAnchor, st.why.c_str(), flees.c_str());
    }
    {
        // M05 "mlog_22890" (22890, Br04): the ring trigger player_near_br04_01 (within 4 km of the dock ring, armed by
        // com_980) and, two comm lines on, dst_to_br04_tradelane (within 3 km of 7890, 25, 56125) with its lane ride
        const u32 br04 = flhash::CreateId("Br04");
        const std::vector<mission::Goal> chain = mission::ChainGoals(22890, br04);
        const mission::Goal* pn = FindG(chain, "player_near_br04_01");
        const mission::Goal* dl = FindG(chain, "dst_to_br04_tradelane");
        CHECK(pn && pn->kind == 1 && pn->objectName == "br04_01_dock_ring" && pn->dist == 4000 && pn->hops == 1 &&
              pn->leads == "mlog_22894");
        CHECK(dl && dl->kind == 2 && dl->dist == 3000 && dl->pos[0] == 7890 && dl->hops == 3 &&
              dl->leads == "mlog_22893" && dl->laneFrom == "Br04_Trade_Lane_Ring_3");
        if (pn && dl)
        {
            CHECK(pn->leadsOrder > dl->leadsOrder && dl->leadsOrder >= 0);
            // the verdict over their real conditions (the dock ring's place and the lane's end are game data: given)
            LookFacts f = base;
            Set3(f.me, 7890, 25, 50000);
            FlyGoal a = FlyGoalOf(*pn, "chain");
            a.placed = true, Set3(a.pos, -38548, 0, -2278);
            FlyGoal b = FlyGoalOf(*dl, "chain");
            b.lane = true, b.laneRideS = 40, Set3(b.laneEnd, -36000, 0, -1000);
            f.flies = {a, b};
            CHECK(LookTarget(f).trigger == "dst_to_br04_tradelane"); // a 40 s ride beats ~66 km of flight (~220 s)
            f.flies[1].laneRideS = 400;
            CHECK(LookTarget(f).trigger == "player_near_br04_01");
        }
    }
    {
        // M04's tethers: juni_tether (juni, Player, 1500, 30, 2500) reported, no FAIL of its own; in Iw03 the bounty
        // hunters' three rows fail through their chain
        const std::string m04 = g_test.install + "\\DATA\\MISSIONS\\M04\\m04.ini";
        const std::vector<mission::Tether> li = mission::Tethers(m04, flhash::CreateId("Li01"));
        bool juni = false;
        for (const mission::Tether& t : li)
            if (t.trigger == "juni_tether")
                juni = t.a == "juni" && t.b == "Player" && t.dist == 1500 && t.secs == 30 && t.inner == 2500 && !t.fails;
        CHECK(juni);
        const std::vector<mission::Tether> iw = mission::Tethers(m04, flhash::CreateId("Iw03"));
        int fails = 0;
        for (const mission::Tether& t : iw)
            fails += t.fails && t.dist == 3000 && t.secs == 30 ? 1 : 0;
        CHECK(iw.size() == 3 && fails == 3);
    }
    {
        // TIMER-DESTROY: M13 "Destroy the Nomads" (24420, St01) advances on the clock and the comm chain, not on NomadCap2's
        // death (cap_timer -> trent_go_back, Cnd_Timer 15 -> ether_one, Cnd_CommComplete 0460 -> go_home; watch_cap2 only
        // hides Cap2); M13 "Destroy the Nomad Battleship" (24425) and M09's generators advance by the kill
        const u32 st01 = flhash::CreateId("St01");
        const mission::StepAdvance cap2 = mission::StepAdvanceOf(24420, st01);
        CHECK(cap2.kind == "timer" && cap2.next == "go_home" && cap2.via == "trent_go_back>ether_one" &&
              cap2.watcher == "watch_cap2");
        // go_home = rep_inst, St01, 24430, 27430, 1658, 0, -2026 (Toledo's docking ring)
        CHECK(cap2.nextText == 24430 && cap2.nextSystem == st01 && cap2.nextHasPos && cap2.nextPos[0] == 1658 &&
              cap2.nextPos[2] == -2026);
        // td_window: go_home's object (St01_dock_ring_1) and Cap2's own point (6350, 0, -5080); cap1's next is Cap2's text
        CHECK(cap2.nextObject == flhash::CreateId("st01_dock_ring_1") && cap2.ownHasPos && cap2.ownPos[0] == 6350);
        CHECK(mission::StepAdvanceOf(24425, st01).nextText == 24420);
        // trent_go_back arms land_on_base (Cnd_BaseEnter st01_01_base -> escort_sinclair_ids): the dock is taken now
        mission::LiveTable lt;
        lt.ok = true;
        lt.script = g_test.install + "\\DATA\\MISSIONS\\M13\\m13.ini";
        mission::LiveTrigger row;
        row.id = flhash::CreateId("land_on_base"), row.state = 1, row.nickname = "land_on_base";
        lt.triggers.push_back(row);
        CHECK(mission::ArmedBaseEnters(lt, 24420).empty()); // (inactive: before trent_go_back)
        lt.triggers[0].state = 0;
        const std::vector<std::string> be = mission::ArmedBaseEnters(lt, 24420);
        CHECK(be.size() == 1 && be[0] == "st01_01_base");
        const mission::StepAdvance cap1 = mission::StepAdvanceOf(24425, st01);
        CHECK(cap1.kind == "kill" && cap1.next == "cap2" && cap1.watcher == "cap_dies");
        // M09 "Go to Tekagi's Arch" (23635, Ku07): the four generators' kill sets "Dock with the Arch"
        const mission::StepAdvance arch = mission::StepAdvanceOf(23635, flhash::CreateId("Ku07"));
        CHECK(arch.kind == "kill" && arch.watcher == "last_generator_dies" && arch.next == "mlog_dock_with_arch");
        const std::vector<std::string> sweep = mission::StepAdvanceSweep(g_test.install + "\\DATA\\MISSIONS");
        int gens = 0, timers = 0;
        for (const std::string& l : sweep)
        {
            gens += l.find("m09.ini ") == 0 && l.find(" kill ") != std::string::npos &&
                            l.find("watcher=last_generator_dies") != std::string::npos
                        ? 1
                        : 0;
            timers += l.find(" timer ") != std::string::npos ? 1 : 0;
            if (g_test.verbose)
                printf("step-advance %s\n", l.c_str());
        }
        CHECK(gens >= 1 && timers >= 1);
        // ALLY-KILL: M13 "Take out the Nomad Ships" (24460, St01) - in_hole_range sets it and arms check_cap (Cnd_Destroyed =
        // NomadCap3, 1, ALL: no killer named), which unlocks St01_to_St03_hole and sets lead_through_gate (rep_inst ...,
        // St01_to_St03_hole): the shape; earlier kill steps are not
        const mission::StepAdvance gate = mission::StepAdvanceOf(24460, st01);
        CHECK(gate.kind == "kill" && gate.watcher == "check_cap" && gate.next == "lead_through_gate" &&
              gate.nextText == 24465);
        CHECK(gate.anyKiller && gate.killObjects.size() == 1 && gate.killObjects[0] == flhash::CreateId("nomadcap3"));
        CHECK(gate.unlocks == flhash::CreateId("st01_to_st03_hole") && gate.unlocks == gate.nextObject);
        CHECK(gate.AllyKillShape());
        CHECK(!cap1.AllyKillShape() && !arch.AllyKillShape() && !cap2.AllyKillShape());
        // the killer field: M06 d_dexter (dexter, 1, EXPLODE, Player) names the player; check_cap names none
        const std::string m06 = g_test.install + "\\DATA\\MISSIONS\\M06\\m06.ini";
        const std::string m13 = g_test.install + "\\DATA\\MISSIONS\\M13\\m13.ini";
        CHECK(mission::AnyKiller("d_dexter", m06) == 0);
        CHECK(mission::AnyKiller("check_cap", m13) == 1);
        CHECK(mission::AnyKiller("in_hole_range", m13) == -1 && mission::AnyKiller("no_such", m13) == -1);
        // (Goal::killer: check_cap's goal as in_hole_range arms it - kind 3, no killer)
        const mission::Goal* cc = nullptr;
        const std::vector<mission::Goal> armed = mission::GoalsArmedBy("in_hole_range", m13, st01);
        cc = FindG(armed, "check_cap");
        CHECK(cc && cc->kind == 3 && cc->killer.empty() && cc->leads == "lead_through_gate");
        // (the sweep: every ally-kill shape of the install)
        int shapes = 0;
        for (const std::string& l : sweep)
        {
            const bool ak = l.find(" ally-kill=1") != std::string::npos;
            shapes += ak ? 1 : 0;
            if (ak && g_test.verbose)
                printf("ally-kill shape %s\n", l.c_str());
        }
        CHECK(shapes >= 1);
    }
    {
        // M13-MIMIC item 3 (docs\research\runners\M13.md "## Plan" 3): the consumable windows and the kill-ahead safety
        const u32 st01 = flhash::CreateId("St01"), st03 = flhash::CreateId("St03"), st03b = flhash::CreateId("St03b");
        bool (*ring)(u32) = [](u32 o) { return o == flhash::CreateId("st01_dock_ring_1"); };
        auto labels = [](const mission::FightsAheadView& v)
        {
            std::string s;
            for (const mission::FightGroup& g : v.groups)
                s += (s.empty() ? "" : " ") + g.label;
            return s;
        };
        // window A: the Nomad battleship (24425) - its own nomadcap only (the caller leaves out the group it fights) before
        // Toledo's ring; Cap2 is a timer step
        const mission::FightsAheadView a = mission::FightsAhead(24425, st01, ring);
        CHECK(a.ok && a.boundary > a.current && labels(a) == "nomadcap");
        CHECK(mission::FightsAhead(24425, st01).boundary > a.boundary); // (no ring test: the Osiris, a base solar)
        // the dock step itself ends its window: "Dock with the Battleship Osiris" (24445) keeps nothing for window B
        const mission::FightsAheadView osirisDock = mission::FightsAhead(24445, st01, ring);
        CHECK(osirisDock.ok && osirisDock.boundary == osirisDock.current && osirisDock.groups.empty());
        // window B: the gate (24460) - its own NomadCap3, then the barrier and the city generators
        const mission::FightsAheadView gate = mission::FightsAhead(24460, st01, ring);
        CHECK(gate.ok && gate.boundary < 0 && labels(gate) == "nomadcap3 generator final_generator");
        // (battery_budget's weights: the kills each group needs - the gate's battleship 1, the barrier's 4, the city's 2)
        CHECK(gate.groups.size() == 3 && gate.groups[0].kills == 1 && gate.groups[1].kills == 4 &&
              gate.groups[2].kills == 2);
        // "Head to the Barrier" (24470) and "Destroy the Generators" (24485): the barrier's and the city's
        CHECK(labels(mission::FightsAhead(24470, st03, ring)) == "generator final_generator");
        CHECK(labels(mission::FightsAhead(24485, st03, ring)) == "generator final_generator");
        // the city's "Fly to the Waypoint" (22235): the city's only - the barrier's are behind it in the file
        CHECK(labels(mission::FightsAhead(22235, st03b, ring)) == "final_generator");
        if (g_test.verbose)
            for (u32 t : {24425u, 24420u, 24440u, 24460u, 24465u, 24470u, 24485u, 22235u})
            {
                const mission::FightsAheadView v = mission::FightsAhead(t, 0, ring);
            {
                printf("fights-ahead %u ok=%d current=%d boundary=%d: %s\n", t, v.ok, v.current, v.boundary,
                       labels(v).c_str());
                for (const mission::FightGroup& fg : v.groups)
                    printf("  %s kills=%d objects=%d\n", fg.label.c_str(), fg.kills, static_cast<int>(fg.objects.size()));
            }
            }
        // kill-ahead: barrier generator 1 before / after juni_near_center fired
        mission::LiveTable lt;
        lt.ok = true;
        lt.script = g_test.install + "\\DATA\\MISSIONS\\M13\\m13.ini";
        auto row = [&](const char* n, int state)
        {
            mission::LiveTrigger r;
            r.id = flhash::CreateId(n), r.state = state, r.nickname = n;
            lt.triggers.push_back(r);
        };
        const u32 gen1 = flhash::CreateId("dyson_generator1");
        row("generators_destroyed", 0); // (in_story; "generator, 4": not fired by the first kill)
        CHECK(mission::KillAheadSafe(lt, gen1, 1).why == "generator1_down not armed"); // (before near_moon)
        row("generator1_down", 0);
        row("juni_near_center", 0);
        const mission::KillSafe early = mission::KillAheadSafe(lt, gen1, 1);
        CHECK(!early.ok && early.why == "generator1_down deacts juni_near_center (-> destroy_generators)");
        lt.triggers.back().state = 2; // Juni reached the centre: "Destroy the Generators" comes in 16.9 s
        CHECK(mission::KillAheadSafe(lt, gen1, 1).ok);
        // the second kill fires generator2_down (not armed in this table: refused) - never the first's again
        CHECK(mission::KillAheadSafe(lt, gen1, 2).why == "generator2_down not armed");
        CHECK(mission::KillAheadSafe(lt, flhash::CreateId("juni"), 1).why == "not a mission solar");
        // kill_ahead's fight-step form at "Defend the Order Base" (24414): nomadcap spawns with cap_dies armed (freighter_dies,
        // m13.ini 1397-1417) - its kill is safe from the spawn; the next step's goals name it (set ab2, 31 Oct 2026: no
        // kill-ahead line at all from Save3989)
        {
            mission::LiveTable cap = lt;
            cap.triggers.clear();
            mission::LiveTrigger r;
            r.id = flhash::CreateId("cap_dies"), r.state = 0, r.nickname = "cap_dies";
            cap.triggers.push_back(r);
            // (the step's setters - exit_base, launch_complete - lead nowhere: no next step to read; the armed rows name it)
            CHECK(mission::NextStepGoals(24414, st01).empty());
            bool named = false;
            for (const mission::Goal& ng : mission::GoalsOfLive(cap, st01, 24414))
            {
                if (g_test.verbose)
                    printf("live 24414: kind=%d trigger=%s object=%s n=%d leads=%s\n", ng.kind, ng.trigger.c_str(),
                           ng.objectName.c_str(), static_cast<int>(ng.objects.size()), ng.leads.c_str());
                named = named || (ng.kind == 3 && std::find(ng.objects.begin(), ng.objects.end(),
                                                            flhash::CreateId("nomadcap")) != ng.objects.end());
            }
            CHECK(named);
            const mission::KillSafe ks = mission::KillAheadSafe(cap, flhash::CreateId("nomadcap"), 1);
            if (g_test.verbose)
                printf("kill-ahead nomadcap at 24414: ok=%d why=%s\n", ks.ok ? 1 : 0, ks.why.c_str());
            CHECK(ks.ok);
        }
        // place_dock (item 6): the escort step's place leads to the dock with the same object
        u32 dockText = 0;
        CHECK(mission::ObjectiveObjectOf("dock_with_osiris", lt.script, &dockText) == flhash::CreateId("osiris") &&
              dockText == 24445);
        CHECK(mission::ObjectiveObjectOf("take_out_nomads", lt.script) == 0);
    }
    mission::UseScriptsFrom("");
}
