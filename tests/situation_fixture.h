// Shared by the situation tests: a synthetic snapshot with every kind of fact, and the recorded fixtures
// (tests\fixtures\situation\*.snap, Save-FLSituation in game; plus fltest.exe -snaps <dir>).
#pragma once
#include "test.h"

#include "../controller/src/impl/situation.h"
#include "../controller/src/impl/situation_io.h"

#include <fstream>
#include <sstream>
#include <windows.h>

// In space near a lane ring, an escort step whose ref resolves to a friendly ship; a hostile attacker, a hostile
// attacking our friend, a ship about to turn hostile, a dead hostile, a script goal per kind, the live table, a leash,
// a nag, waypoints, spawns - and strings that need encoding.
inline situation::Snapshot SyntheticSnapshot()
{
    using namespace situation;
    Snapshot s;
    s.objSeq = 7;
    s.step.textId = 23270;
    s.step.refs[0] = 8, s.step.refObj[0] = 101, s.step.refCount = 1;
    s.step.verb = Verb::Escort;
    s.entries.push_back(s.step);
    SnapStep e2;
    e2.textId = 23265, e2.hasNav = true, e2.nav[0] = -1234.5f, e2.nav[1] = 7, e2.nav[2] = 98765.25f, e2.navSystem = 99;
    e2.verb = Verb::Dock;
    s.entries.push_back(e2);
    s.title = 23000;
    s.marks = {202, 101};
    s.objDefs.push_back({"nn_defend", "rep_inst", "M07\\m07.ini"});
    SnapGoal g1;
    g1.kind = 1, g1.object = 404, g1.objectName = "Li01_Trade_Lane_Ring_1", g1.dist = 3000, g1.subject = "player";
    g1.trigger = "near_ring", g1.leads = "nn_dock", g1.file = "M07\\m07.ini", g1.placed = true;
    g1.objPos[0] = 500, g1.objPos[1] = 0, g1.objPos[2] = 0;
    s.goals.push_back(g1);
    SnapGoal g3;
    g3.kind = 3, g3.objectName = "gens", g3.count = 1, g3.subject = "player", g3.trigger = "gens_dead", g3.leads = "SUCCEED";
    g3.file = "M07\\m07.ini", g3.byEntry = true;
    g3.objects = {{606, true}, {607, false}};
    s.goals.push_back(g3);
    SnapGoal g2;
    g2.kind = 2, g2.pos[0] = 100000, g2.dist = 500, g2.subject = "player", g2.trigger = "far_point", g2.leads = "nn_far";
    s.nextGoals.push_back(g2);
    s.nextObjective = "nn_dock";
    s.goalsScript = "M07\\m07.ini";
    s.leashes.push_back({101, "juni", 6000, 21001, {111, 222}});
    s.nag.has = true, s.nag.object = 404, s.nag.name = "Li01_Trade_Lane_Ring_1", s.nag.dist = 0;
    s.pendingHostile = {505};
    s.liveOk = true, s.liveReason = "", s.liveScript = "M07\\m07.ini", s.liveController = 4242, s.liveMs = 0.125f;
    s.live.push_back({1, 0, "Cnd_DistShip", "near_ring", "cnd_distship"});
    s.live.push_back({2, 2, "Cnd_Destroyed", "gens_dead", "cnd_destroyed"});
    s.live.push_back({3, 1, "", "", ""});
    s.system = 2610157064u, s.inSpace = true, s.clock = 1234.5678901234;
    s.pilotOk = true, s.op = 2, s.cruise = true, s.target = 202;
    s.vel[0] = 80;
    s.hull = 0.75f, s.shield = 1;
    s.knowledge = 1;
    s.preAggro = 4000;
    s.apMode = 3, s.apTarget = 101, s.objOn = true;
    const auto contact = [&](u32 id, bool ship, float dist, float att)
    {
        SnapContact c;
        c.id = id, c.ship = ship, c.distance = dist, c.attitude = att, c.attitudeAsked = ship, c.health = 1;
        c.pos[0] = dist, c.maxHit = 1000;
        s.contacts.push_back(c);
        return &s.contacts.back();
    };
    contact(101, true, 1000, 0.8f);
    contact(202, true, 2000, -1)->targetsPlayer = true;
    contact(303, true, 8000, -1)->targetId = 101;
    contact(505, true, 4000, -0.65f)->preHostile = true;
    contact(606, false, 20000, 0)->health = 0.5f;
    contact(707, true, 1500, -1)->health = 0; // dead
    SnapContact* ring = contact(404, false, 500, 0);
    ring->type = 1u << 7, ring->dock = true, ring->attKnown = true, ring->att = 0.2f;
    SnapContact* odd = contact(808, false, 3000, 0);
    odd->maxHit = INFINITY, odd->health = -1;
    s.lanes.push_back({404, 405, 0, 1, -1});
    s.waypoints.push_back({{1, 2, 3}, 99, 0});
    s.spawns.push_back({505, 777, {4000, 0, 0}});
    s.spawnCount = 1;
    // v3: the rules' facts, the memory, the old decision
    s.refsObj = {101, 202};
    s.goals[1].objects[0].pos[0] = 20000.5f;
    s.leashes[0].off = true, s.leashes[0].ship = 101, s.leashes[0].placed = true, s.leashes[0].opos[2] = -5.25f;
    SnapArmed a;
    a.by = "near_ring", a.byFile = "M07\\m07.ini", a.goal = g2;
    s.armed.push_back(a);
    s.hasEntry = true, s.entry[0] = 12.5f;
    s.tractorRange = 1500, s.tractorExitRun = true, s.tractorExitRange = 600;
    s.queue.seq = 7, s.queue.mode = "escort", s.queue.target = 101, s.queue.sub = "escort", s.queue.subTarget = 101;
    s.queue.action = "escort", s.queue.trHasDest = true, s.queue.trDest[1] = -3.5f, s.queue.lockId = ~0u;
    s.queue.leashShip = 101, s.queue.leashDist = 6000, s.queue.lastDock = 404, s.queue.badGoal = "bad goal";
    s.queue.deadGoals = {"dead,one", "dead_two"}, s.queue.scriptMarks = {606}, s.queue.seenAlive = {606, 607};
    s.decided.rule = "leash", s.decided.mode = "travel", s.decided.action = "leash";
    // v4: the S4 rules' facts and memory
    SnapAfter af;
    af.leads = "nm_next", af.file = "M07\\m07.ini", af.goal = g2;
    s.after.push_back(af);
    s.prestage.lastReal = 23100, s.prestage.next = "nn next, one", s.prestage.nextOk = true, s.prestage.nextPos[2] = -7.75f;
    s.prestage.nextText = 23105, s.prestage.stageFrom = 23100, s.prestage.stageOk = false;
    s.realNext.push_back(g2);
    s.titleEscape = true, s.pilotSystem = 2745692751u;
    s.bigCoast = false, s.survivalCalm = true, s.survivalStage = false, s.survivalUntilJump = true;
    s.contacts[0].radius = 812.5f, s.contacts[1].named = true;
    s.queue.lastReal = 23090, s.queue.arrived = true, s.queue.stepAgeMs = 125000.25, s.queue.escorted = 101;
    s.queue.survTitle = 22602, s.queue.survMission = true, s.queue.survEnded = false, s.queue.survSystem = 77;
    s.queue.survival = true;
    // v5: the S5 rules' facts and memory
    s.entries[1].refPlaced = false;
    s.step.refPlaced = true, s.step.refPos[0] = 1000, s.step.refPos[2] = -0.5f;
    s.entries[0] = s.step;
    s.contacts[6].dockRing = 409; // (the ring)
    s.lanes[0].nextPlaced = true, s.lanes[0].nextPos[0] = 7500.25f;
    s.followDistance = 1250;
    s.queue.missionLeader = 101, s.queue.viaNext = true, s.queue.arriveRetries = 3, s.queue.trPassRing = 404;
    return s;
}

inline std::string ReadTextFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

struct SnapFixture
{
    std::string name, path, text;
};
// Every *.snap in `dir` (sorted by name).
inline std::vector<SnapFixture> SnapFixtures(const std::string& dir)
{
    std::vector<SnapFixture> out;
    WIN32_FIND_DATAA d;
    HANDLE h = FindFirstFileA((dir + "\\*.snap").c_str(), &d);
    if (h == INVALID_HANDLE_VALUE)
        return out;
    do
        out.push_back({d.cFileName, dir + "\\" + d.cFileName, ReadTextFile(dir + "\\" + d.cFileName)});
    while (FindNextFileA(h, &d));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const SnapFixture& a, const SnapFixture& b) { return a.name < b.name; });
    return out;
}

// A comment line's value: "#<key> <value ...>" -> "<value ...>" ("" when absent).
inline std::string SnapComment(const std::string& text, const std::string& key)
{
    const std::string want = "#" + key + " ";
    size_t a = 0;
    while (a < text.size())
    {
        size_t b = text.find('\n', a);
        if (b == std::string::npos)
            b = text.size();
        std::string line = text.substr(a, b - a);
        while (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.compare(0, want.size(), want) == 0)
            return line.substr(want.size());
        a = b + 1;
    }
    return "";
}
