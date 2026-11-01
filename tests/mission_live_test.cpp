// core\game\mission_live.cpp's pure pieces without the game: the walk root -> Server.dll's controller list -> mission ->
// active list over a fake memory (the stale-id rule, a foreign controller with the same id, torn / endless lists,
// unreadable nodes), the vtable -> Cnd_ name map (build-keyed, verified rows only), and naming the ids of SaveS182 (M12.hole,
// research\evidence\triggers\SaveS182_triggers.txt) by the install's scripts (mission::NameTriggers).
#include "test.h"

#include "game/flhash.h"
#include "game/mission.h"
#include "game/mission_live.h"

#include <map>

namespace
{
typedef std::map<u32, u32> Mem32; // address -> word (every read must hit whole words)

bool FakeRead(uintptr_t addr, void* out, size_t len, void* ctx)
{
    const Mem32& m = *static_cast<const Mem32*>(ctx);
    if (len % 4)
        return false;
    for (size_t i = 0; i < len; i += 4)
    {
        const auto it = m.find(static_cast<u32>(addr + i));
        if (it == m.end())
            return false;
        memcpy(static_cast<char*>(out) + i, &it->second, 4);
    }
    return true;
}

const u32 kContent = 0x06EA0000, kOther = 0x06D00000;

struct World
{
    Mem32 m;
    mission::live::Layout l;
    u32 storyId = 0x40000123;
    // addresses
    const u32 storyAt = kContent + 0x12EE14, ctlHeadAt = 0x06CE0000 + 0xAF644;
    const u32 ctlHead = 0x100000, ctlA = 0x100100, ctlB = 0x100200;
    const u32 inst = 0x200000;
    const u32 listHead = 0x300000;
    std::vector<u32> trigIds = {flhash::CreateId("tr_to_st01"), flhash::CreateId("king_dies"),
                                flhash::CreateId("decloak_all")};

    World()
    {
        l.content = kContent, l.contentEnd = kContent + 0x145000;
        l.storyIdAt = storyAt, l.ctlHeadAt = ctlHeadAt;
        l.listHeadOff = 0x1C8, l.listValOff = 8, l.idOff = 0xC, l.recOff = 0x10, l.condOff = 4;
        m[storyAt] = storyId;
        // Server.dll: head -> A (another module's controller) -> B (the story mission) -> head
        m[ctlHeadAt] = ctlHead;
        m[ctlHead] = ctlA;
        Ctl(ctlA, ctlB, 0x7, kOther, 0x250000);
        Ctl(ctlB, ctlHead, storyId, kContent, inst);
        m[inst + 4] = storyId;
        Triggers(trigIds);
    }
    void Ctl(u32 node, u32 next, u32 id, u32 mod, u32 instance)
    {
        m[node] = next;
        m[node + 8] = instance;
        m[node + 0xC] = mod;
        m[node + 0x18] = id;
    }
    // the active list: head sentinel, one list node per trigger -> its map node -> its condition (vtable Cnd_Timer)
    void Triggers(const std::vector<u32>& ids)
    {
        m[inst + 0x1C8] = listHead;
        m[inst + 0x1CC] = static_cast<u32>(ids.size());
        u32 prev = listHead;
        for (size_t i = 0; i < ids.size(); ++i)
        {
            const u32 e = 0x310000 + static_cast<u32>(i) * 0x100, node = 0x320000 + static_cast<u32>(i) * 0x100,
                      cond = 0x330000 + static_cast<u32>(i) * 0x100;
            m[prev] = e;
            m[e + 8] = node;
            m[node + 0xC] = ids[i];
            m[node + 0x10] = 0; // armed
            m[node + 0x14] = cond;
            m[cond] = kContent + 0x115884; // Cnd_Timer's vtable
            m[cond + 4] = inst;            // its mission
            float elapsed = 0.25f * static_cast<float>(i), duration = 10;
            memcpy(&m[cond + 0xC], &elapsed, 4);
            memcpy(&m[cond + 0x10], &duration, 4);
            prev = e;
        }
        m[prev] = listHead;
    }
    const char* Walk(u32& ctl, std::vector<mission::live::Raw>& out)
    {
        return mission::live::Walk(l, FakeRead, &m, ctl, out);
    }
};
} // namespace

void Test_mission_live()
{
    using mission::live::Raw;
    {
        World w;
        u32 ctl = 0;
        std::vector<Raw> out;
        CHECK(w.Walk(ctl, out) == nullptr);
        CHECK(ctl == w.storyId);
        CHECK(out.size() == 3);
        if (out.size() == 3)
        {
            CHECK(out[0].id == w.trigIds[0] && out[2].id == w.trigIds[2]);
            CHECK(out[1].state == 0);
            CHECK(out[1].condVt == kContent + 0x115884);
            float e = 0;
            memcpy(&e, &out[1].f[0], 4);
            CHECK_NEAR(e, 0.25, 1e-6);
        }
    }
    {
        // the stale-id rule: the global still names the ended mission's id, no controller of Content.dll carries it
        World w;
        w.m[w.storyAt] = 0x40000999;
        u32 ctl = 0;
        std::vector<Raw> out;
        const char* why = w.Walk(ctl, out);
        CHECK(why && strstr(why, "stale"));
        CHECK(out.empty());
    }
    {
        // the same id held by another module's controller is not the story mission
        World w;
        w.m[w.ctlB + 0xC] = kOther;
        u32 ctl = 0;
        std::vector<Raw> out;
        const char* why = w.Walk(ctl, out);
        CHECK(why && strstr(why, "stale"));
    }
    {
        // the controller list empty (only the sentinel)
        World w;
        w.m[w.ctlHead] = w.ctlHead;
        u32 ctl = 0;
        std::vector<Raw> out;
        CHECK(w.Walk(ctl, out) != nullptr);
    }
    {
        // no story mission yet (a fresh Content.dll: the global is zero)
        World w;
        w.m[w.storyAt] = 0;
        u32 ctl = 0;
        std::vector<Raw> out;
        const char* why = w.Walk(ctl, out);
        CHECK(why && strstr(why, "no story mission"));
    }
    {
        // the instance does not carry the id (not a mission object)
        World w;
        w.m[w.inst + 4] = 5;
        u32 ctl = 0;
        std::vector<Raw> out;
        CHECK(w.Walk(ctl, out) != nullptr);
        CHECK(out.empty());
    }
    {
        // a torn active list: one more node than its size says / an endless loop / an unreadable node
        World w;
        w.m[w.inst + 0x1CC] = 2;
        u32 ctl = 0;
        std::vector<Raw> out;
        CHECK(w.Walk(ctl, out) != nullptr);
        World x;
        x.m[0x310000 + 0x200] = 0x310000; // the last node leads back to the first
        CHECK(x.Walk(ctl, out) != nullptr);
        World y;
        y.m.erase(0x320000 + 0x100 + 0xC); // the second trigger's id unreadable
        CHECK(y.Walk(ctl, out) != nullptr);
        World z;
        z.m[z.inst + 0x1CC] = 4; // fewer nodes than the size
        CHECK(z.Walk(ctl, out) != nullptr);
        World b; // a condition that is not this mission's (another object read as a mission)
        b.m[0x330000 + 0x100 + 4] = 0x250000;
        const char* why = b.Walk(ctl, out);
        CHECK(why && strstr(why, "point back"));
        World c; // an endless controller list
        c.m[c.ctlB] = c.ctlA;
        c.m[c.ctlB + 0x18] = 1; // (not the story id, so the walk goes round)
        CHECK(c.Walk(ctl, out) != nullptr);
    }
    {
        // an empty active list is a valid answer (a mission with nothing armed)
        World w;
        w.Triggers({});
        u32 ctl = 0;
        std::vector<Raw> out;
        CHECK(w.Walk(ctl, out) == nullptr);
        CHECK(out.empty());
    }
    {
        // the vtable map: Freelancer 1.1's build only, verified rows named, scripted rows not
        int status = -1;
        CHECK(mission::live::CondName(0x3ECBB142, 0x145000, 0x115884, &status) &&
              strcmp(mission::live::CondName(0x3ECBB142, 0x145000, 0x115884), "Cnd_Timer") == 0 && status >= 1);
        CHECK(mission::live::CondName(0x3E401CD4, 0x144000, 0x115884) == nullptr); // v1.0: other RVAs
        CHECK(mission::live::CondName(0x3ECBB142, 0x145000, 0x123456) == nullptr);
        status = -1; // a scripted-only row (Cnd_BaseExit): known, not named
        CHECK(mission::live::CondName(0x3ECBB142, 0x145000, 0x115744, &status) == nullptr && status == 0);
        CHECK(mission::live::CondName(0x3ECBB142, 0x145000, 0x1151B0, &status) && status == 2); // SpaceEnter, in game
        CHECK(mission::live::CondName(0x3ECBB142, 0x145000, 0) == nullptr);
    }
    // fail closed without the game: no Content.dll in this process
    {
        const mission::LiveTable t = mission::LiveTriggers();
        CHECK(!t.ok && t.triggers.empty() && !t.reason.empty());
    }
    // naming: SaveS182's ten [TriggerSave] ids are M12's triggers (the install's scripts)
    if (NeedInstall("mission_live naming (DATA\\MISSIONS)"))
    {
        const std::vector<u32> ids = {2466975689u, 2468953796u, 2450679565u, 2330263811u, 2597708552u,
                                      2811127236u, 2976437447u, 3008764548u, 2274259396u, 3014659329u};
        std::string script;
        std::vector<std::string> nick, cond;
        CHECK(mission::NameTriggers(ids, script, nick, cond, g_test.install + "\\DATA\\MISSIONS"));
        CHECK(script.find("m12.ini") != std::string::npos || script.find("M12.ini") != std::string::npos);
        CHECK(nick.size() == ids.size());
        if (nick.size() == ids.size())
        {
            CHECK(nick[0] == "king_dies" && cond[0] == "cnd_destroyed");
            CHECK(nick[3] == "tr_to_st01" && cond[3] == "cnd_timer");
            CHECK(nick[9] == "timer_setnnobj_assist_von_claussen" && cond[9] == "cnd_commcomplete");
            for (size_t i = 0; i < ids.size(); ++i)
                CHECK(!nick[i].empty() && flhash::CreateId(nick[i]) == ids[i]);
        }
        std::string none;
        CHECK(!mission::NameTriggers({0x12345678u}, none, nick, cond, g_test.install + "\\DATA\\MISSIONS"));
    }
}
