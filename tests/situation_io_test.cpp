// The snapshot's text form (controller\src\impl\situation_io.h, `snap v5`; v1-v4 texts: mission_live_goals_test, v3 / v4
// rewrites of the synthetic one and the v1-v4 fixtures): the round trip is exact (text -> snapshot
// -> text identical, floats bit for bit, NaN / infinity / negative zero, encoded strings), comments and CR are skipped,
// and every kind of damage fails closed (no header, a cut text, an unknown record or field, a field out of order or
// missing, a bad value, a single record repeated or missing, text after the end); the recorded fixtures round-trip too.
#include "situation_fixture.h"

#include <cstring>

void Test_situation_io()
{
    using namespace situation;
    Snapshot s = SyntheticSnapshot();
    s.liveReason = "no story mission: id 0 = stale, a=b, 100% \"quoted\"\ttab";
    s.contacts[0].attitude = -0.0f;
    s.contacts[1].health = NAN;
    const std::string text = Serialize(s);
    Snapshot back;
    std::string err;
    CHECK(Parse(text, back, &err));
    if (!err.empty())
        fprintf(stderr, "  situation_io: %s\n", err.c_str());
    CHECK(Serialize(back) == text);
    CHECK(back.liveReason == s.liveReason);
    CHECK(back.contacts.size() == s.contacts.size() && back.entries.size() == 2 && back.goals.size() == 2);
    CHECK(back.contacts.size() > 7 && std::isinf(back.contacts[7].maxHit) && std::isnan(back.contacts[1].health));
    CHECK(std::signbit(back.contacts[0].attitude));
    CHECK(back.entries[1].nav[2] == s.entries[1].nav[2] && back.clock == s.clock);
    CHECK(back.step.refCount == 1 && back.step.refObj[0] == 101 && back.step.verb == Verb::Escort);
    CHECK(back.goals[1].objects.size() == 2 && back.goals[1].objects[0].placed && !back.goals[1].objects[1].placed);
    CHECK(back.leashes.size() == 1 && back.leashes[0].offComms.size() == 2);
    // v3: the rules' facts, the memory, the old decision
    CHECK(back.goals[1].objects[0].pos[0] == 20000.5f && back.refsObj == s.refsObj);
    CHECK(back.leashes[0].off && back.leashes[0].ship == 101 && back.leashes[0].placed && back.leashes[0].opos[2] == -5.25f);
    CHECK(back.armed.size() == 1 && back.armed[0].by == "near_ring" && back.armed[0].goal.trigger == "far_point");
    CHECK(back.hasEntry && back.entry[0] == 12.5f && back.tractorRange == 1500 && back.tractorExitRun);
    CHECK(back.queue.deadGoals == s.queue.deadGoals && back.queue.badGoal == "bad goal" && back.queue.lockId == ~0u);
    CHECK(back.queue.seenAlive == s.queue.seenAlive && back.queue.trDest[1] == -3.5f && back.queue.mode == "escort");
    CHECK(back.decided.rule == "leash" && back.decided.mode == "travel" && back.decided.action == "leash");
    // v4: the S4 rules' facts and memory
    CHECK(back.after.size() == 1 && back.after[0].leads == "nm_next" && back.after[0].goal.trigger == "far_point");
    CHECK(back.prestage.lastReal == 23100 && back.prestage.next == "nn next, one" && back.prestage.nextOk &&
          back.prestage.nextPos[2] == -7.75f && back.prestage.nextText == 23105 && !back.prestage.stageOk);
    CHECK(back.realNext.size() == 1 && back.titleEscape && back.pilotSystem == 2745692751u && !back.bigCoast &&
          !back.survivalStage);
    CHECK(back.contacts[0].radius == 812.5f && back.contacts[1].named && !back.contacts[0].named);
    CHECK(back.queue.lastReal == 23090 && back.queue.arrived && back.queue.stepAgeMs == 125000.25 &&
          back.queue.escorted == 101 && back.queue.survTitle == 22602 && back.queue.survMission &&
          back.queue.survSystem == 77 && back.queue.survival);
    // v5: the S5 rules' facts and memory
    CHECK(back.step.refPlaced && back.step.refPos[0] == 1000 && back.step.refPos[2] == -0.5f && !back.entries[1].refPlaced);
    CHECK(back.contacts[6].dockRing == 409 && back.lanes[0].nextPlaced && back.lanes[0].nextPos[0] == 7500.25f &&
          !back.lanes[0].prevPlaced && back.followDistance == 1250);
    CHECK(back.queue.missionLeader == 101 && back.queue.viaNext && back.queue.arriveRetries == 3 &&
          back.queue.trPassRing == 404);
    // every float bit for bit (a sample of awkward values through one field)
    for (float v : {0.1f, 1.0f / 3.0f, 123456.789f, -98765.4321f, 1e-30f, 3.4e38f, 1.17549435e-38f})
    {
        Snapshot a = s;
        a.pos[1] = v;
        Snapshot b2;
        CHECK(Parse(Serialize(a), b2) && memcmp(&b2.pos[1], &v, sizeof(v)) == 0);
    }
    // the empty snapshot
    {
        Snapshot e, e2;
        CHECK(Parse(Serialize(e), e2, &err) && Serialize(e2) == Serialize(e));
    }
    // comments, CRLF, a trailing comment after the end
    {
        std::string t;
        for (char c : text)
            t += c == '\n' ? std::string("\r\n") : std::string(1, c);
        t = "# recorded somewhere\r\n" + t + "#game-hash 0x12345678\r\n";
        Snapshot c;
        CHECK(Parse(t, c, &err) && Serialize(c) == text);
    }
    // damage fails closed
    const auto fails = [&](const std::string& t, const char* why)
    {
        Snapshot x;
        std::string e;
        const bool ok = Parse(t, x, &e);
        if (ok)
            fprintf(stderr, "  situation_io: parsed although %s\n", why);
        return !ok && !e.empty();
    };
    const auto replace = [&](const std::string& from, const std::string& to)
    {
        std::string t = text;
        const size_t at = t.find(from);
        CHECK(at != std::string::npos); // (the damage must apply: a missing anchor would pass vacuously)
        return at == std::string::npos ? std::string() : t.replace(at, from.size(), to);
    };
    CHECK(fails("", "empty"));
    CHECK(fails(text.substr(0, text.size() / 2), "cut in half"));
    CHECK(fails(replace("end\n", ""), "no end"));
    CHECK(fails(replace("snap v5", "snap v6"), "a later version"));
    CHECK(fails(replace("snap v5", "snap v4"), "a v5 text under a v4 header (its v5 fields are unknown there)"));
    CHECK(fails(replace("snap v5", "snap v3"), "a v5 text under a v3 header"));
    CHECK(fails(replace("snap v5", "snap v2"), "a v5 text under a v2 header"));
    CHECK(fails(replace("snap v5", "snap v1"), "a v5 text under a v1 header"));
    CHECK(fails(replace(" last_real=23090", " last_real=23090 extra=1"), "a v4 queue field too many"));
    CHECK(fails(replace(" radius=812.5 named=0", " named=0 radius=812.5"), "v4 contact fields out of order"));
    CHECK(fails(replace(" via_next=1 arrive_retries=3", " arrive_retries=3 via_next=1"), "v5 queue fields out of order"));
    CHECK(fails(replace(" follow_distance=1250", ""), "a v5 setting missing"));
    CHECK(fails(replace(" next_placed=1", " next_placed=2"), "a v5 lane flag not 0 / 1"));
    CHECK(fails(replace(" dock_ring=409", " dock_ring=-409"), "a negative v5 dock ring"));
    {
        const size_t at = text.find("\nprestage ");
        const size_t end = text.find('\n', at + 1);
        CHECK(at != std::string::npos && fails(text.substr(0, at + 1) + text.substr(end + 1), "prestage missing"));
        CHECK(fails(text.substr(0, end + 1) + text.substr(at + 1, end - at) + text.substr(end + 1), "prestage twice"));
    }
    // a v3 text (the S3 fixture's form) still parses and writes back byte for byte, without the v4 records
    {
        Snapshot v3 = s;
        v3.version = 3;
        const std::string t3 = Serialize(v3);
        Snapshot b3;
        CHECK(t3.find("\nprestage ") == std::string::npos && t3.find(" last_real=") == std::string::npos);
        CHECK(Parse(t3, b3, &err) && b3.version == 3 && Serialize(b3) == t3 && b3.after.empty() && !b3.queue.survival);
    }
    // ...and a v4 text (the S4 fixtures' form), without the v5 fields
    {
        Snapshot v4 = s;
        v4.version = 4;
        const std::string t4 = Serialize(v4);
        Snapshot b4;
        CHECK(t4.find(" ref_placed=") == std::string::npos && t4.find(" mission_leader=") == std::string::npos &&
              t4.find(" dock_ring=") == std::string::npos && t4.find(" next_placed=") == std::string::npos &&
              t4.find(" follow_distance=") == std::string::npos && t4.find("\nprestage ") != std::string::npos);
        CHECK(Parse(t4, b4, &err) && b4.version == 4 && Serialize(b4) == t4 && b4.queue.missionLeader == 0 &&
              b4.contacts[6].dockRing == 0 && b4.followDistance == 1500 && b4.queue.survival);
    }
    CHECK(fails(replace(" hops=", " hopz="), "a v2 field renamed"));
    CHECK(fails(replace(" refs_obj=", " refs_objs="), "a v3 field renamed"));
    CHECK(fails(replace(" objects=606:1:", " objects=606:1;"), "a v3 goal object without its position"));
    {
        const size_t at = text.find("\nqueue ");
        const size_t end = text.find('\n', at + 1);
        CHECK(at != std::string::npos && fails(text.substr(0, at + 1) + text.substr(end + 1), "queue missing"));
        const size_t d = text.find("\ndecided ");
        const size_t de = text.find('\n', d + 1);
        CHECK(d != std::string::npos && fails(text.substr(0, d + 1) + text.substr(de + 1), "decided missing"));
    }
    CHECK(fails(text + "contact id=1\n", "text after end"));
    CHECK(fails(replace("\nplace ", "\nplaces "), "unknown record"));
    CHECK(fails(replace(" hull=", " hull2="), "unknown field name"));
    CHECK(fails(replace(" base=0 space=1", " space=1 base=0"), "fields out of order"));
    CHECK(fails(replace(" clock=", " extra=1 clock="), "an extra field"));
    CHECK(fails(replace(" title=23000", ""), "a missing field"));
    CHECK(fails(replace(" seq=7", " seq=-7"), "a negative u32"));
    CHECK(fails(replace(" seq=7", " seq=7x"), "a number with junk"));
    CHECK(fails(replace(" space=1", " space=2"), "a bool not 0 / 1"));
    CHECK(fails(replace(" verb=escort", " verb=wander"), "an unknown verb"));
    CHECK(fails(replace(" refs=8 ref_obj=101", " refs=8 ref_obj=101,102"), "refs and their objects differ"));
    CHECK(fails(replace(" refs=8", " refs=1,2,3,4,5"), "more than 4 refs"));
    CHECK(fails(replace(" objects=606:1:", " objects=606:2:"), "a bad placed flag"));
    CHECK(fails(replace("%25", "%2"), "a broken escape"));
    {
        const size_t at = text.find("\nexec ");
        const size_t end = text.find('\n', at + 1);
        CHECK(fails(text.substr(0, end + 1) + text.substr(at + 1, end - at) + text.substr(end + 1), "exec twice"));
        CHECK(fails(text.substr(0, at + 1) + text.substr(end + 1), "exec missing"));
    }
    // the recorded fixtures round-trip
    int n = 0;
    for (const std::string& dir : {g_test.fixtures + "\\situation", g_test.snaps})
        for (const SnapFixture& f : dir.empty() ? std::vector<SnapFixture>() : SnapFixtures(dir))
        {
            Snapshot x;
            std::string e;
            const bool ok = Parse(f.text, x, &e);
            if (!ok)
                fprintf(stderr, "  situation_io: %s: %s\n", f.name.c_str(), e.c_str());
            CHECK(ok);
            Snapshot y;
            CHECK(ok && Parse(Serialize(x), y) && Serialize(y) == Serialize(x));
            ++n;
        }
    if (g_test.verbose)
        printf("  %d recorded snapshots round-tripped\n", n);
}
