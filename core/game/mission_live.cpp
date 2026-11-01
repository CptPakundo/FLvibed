// The story mission's live trigger table (mission_live.h; docs\FREELANCER_RE.md "The live trigger table").
#include "game/mission_live.h"
#include "game/mission.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"

#include <sstream>

namespace mission
{
namespace live
{
static const u32 kMaxControllers = 4096, kMaxTriggers = 2048; // bounded walks (a torn list never loops)

const char* Walk(const Layout& l, ReadFn read, void* ctx, u32& controller, std::vector<Raw>& out)
{
    out.clear();
    controller = 0;
    // (32-bit game pointers read as u32 whatever the reader's own pointer size)
    const auto rd = [&](uintptr_t a, u32& v) { return a && read(a, &v, sizeof v, ctx); };
    u32 id = 0;
    if (!rd(l.storyIdAt, id))
        return "the story id global is unreadable";
    controller = id;
    if (!id)
        return "no story mission (story id 0: none started since Content.dll loaded)";
    // Server.dll's controller list (MSVC6 std::list: the head is a sentinel, node +0 next; the value from +8)
    u32 head = 0, n = 0, inst = 0;
    if (!rd(l.ctlHeadAt, head) || !rd(head, n))
        return "Server.dll's controller list is unreadable";
    for (u32 g = 0; n != head; ++g)
    {
        if (g >= kMaxControllers)
            return "Server.dll's controller list does not end";
        u32 nid = 0, mod = 0;
        if (!rd(n + l.ctlIdOff, nid) || !rd(n + l.ctlModOff, mod))
            return "a Server.dll controller node is unreadable";
        if (nid == id && mod == static_cast<u32>(l.content))
        {
            if (!rd(n + l.ctlInstOff, inst) || !inst)
                return "the story controller's instance is unreadable";
            break;
        }
        if (!rd(n, n))
            return "a Server.dll controller node is unreadable";
    }
    // the global is never cleared: after the mission's controller is destroyed it names nothing (or, never seen, another
    // module's controller that got the id) - only a Content.dll controller of that id counts
    if (!inst)
        return "the story id is not in Server.dll's controller list (stale: the story mission has ended)";
    u32 own = 0;
    if (!rd(inst + l.ownIdOff, own) || own != id)
        return "the story controller's instance does not carry its id (not a mission object)";
    // the active list: std::list<map node*> (head pointer at listHeadOff, size right after it)
    u32 lh = 0, size = 0, e = 0;
    if (!rd(inst + l.listHeadOff, lh) || !rd(inst + l.listHeadOff + 4, size) || !rd(lh, e))
        return "the active trigger list is unreadable";
    if (size > kMaxTriggers)
        return "the active trigger list's size is implausible";
    for (u32 i = 0; e != lh; ++i)
    {
        if (i >= size)
            return "the active trigger list is longer than its size (changed while read)";
        Raw r;
        u32 node = 0, state = 0, cond = 0;
        if (!rd(e + l.listValOff, node) || !rd(node + l.idOff, r.id) || !rd(node + l.recOff, state) ||
            !rd(node + l.recOff + l.condOff, cond))
            return "a trigger node is unreadable";
        r.state = static_cast<int>(state);
        // every Condition points back to its mission (cond +4): what proves the object is a mission and the node its own
        u32 vt = 0, back = 0;
        if (!cond || !rd(cond + l.condBackOff, back) || back != inst)
            return "a trigger's condition does not point back to the mission (not a mission object)";
        if (rd(cond, vt))
        {
            r.cond = cond;
            r.condVt = vt;
            if (!read(cond + 0xC, r.f, sizeof r.f, ctx))
                r.f[0] = r.f[1] = 0;
        }
        out.push_back(r);
        if (!rd(e, e))
            return "a trigger list node is unreadable";
    }
    if (out.size() != size)
        return "the active trigger list is shorter than its size (changed while read)";
    return nullptr;
}

// Content.dll's Condition vtables by RVA, Freelancer 1.1's build (TimeDateStamp 0x3ECBB142, SizeOfImage 0x145000: the
// vanilla 1.1 and HD Edition 0.7.1 / upstream files differ in bytes but every vtable sits at the same RVA; v1.0 differs).
// Mapped by a script from each class's constructor (research\evidence\triggers\cnd_act_vtables.txt); status 1 = read in
// the disassembly (the Save / Load / Reset slots), 2 = verified in game (the live table's vtable against the trigger's
// ini condition, 29 Oct 2026, research\experience\runs\a39_live_triggers_2026-10-29), 0 = scripted only (not named).
struct CondVt
{
    u32 rva;
    const char* name;
    int status;
};
static const CondVt kCond11[] = {
    {0x115770, "Cnd_BaseEnter", 2},     {0x115744, "Cnd_BaseExit", 0},       {0x115710, "Cnd_CargoScanned", 0},
    {0x1156E4, "Cnd_CharSelect", 2},    {0x1156B4, "Cnd_CommComplete", 2},   {0x115688, "Cnd_CmpToPlane", 0},
    {0x115640, "Cnd_Destroyed", 2},     {0x115614, "Cnd_DistCircle", 0},     {0x1155E8, "Cnd_DistShip", 2},
    {0x1155C0, "Cnd_DistVec", 2},       {0x115594, "Cnd_DistVecLbl", 0},     {0x115568, "Cnd_EncLaunched", 0},
    {0x115540, "Cnd_HasMsn", 0},        {0x115514, "Cnd_HealthDec", 0},      {0x1154EC, "Cnd_InSpace", 0},
    {0x1154C0, "Cnd_InTradelane", 0},   {0x115498, "Cnd_InZone", 0},         {0x11546C, "Cnd_JumpgateAct", 0},
    {0x11543C, "Cnd_JumpInComplete", 0}, {0x11540C, "Cnd_LaunchComplete", 0}, {0x1153E0, "Cnd_LocEnter", 2},
    {0x1153B8, "Cnd_LocExit", 0},       {0x115388, "Cnd_LootAcquired", 0},   {0x11535C, "Cnd_MsnResponse", 0},
    {0x1159E4, "Cnd_NPCSystemEnter", 0}, {0x11565C, "Cnd_NPCSystemExit", 0}, {0x1152E8, "Cnd_PlayerLaunch", 0},
    {0x1152B8, "Cnd_PlayerManeuver", 0}, {0x11528C, "Cnd_PopUpDialog", 0},  {0x115264, "Cnd_ProjHit", 0},
    {0x115230, "Cnd_ProjHitShipToLbl", 0}, {0x115208, "Cnd_RTCDone", 2},    {0x1151DC, "Cnd_RumorHeard", 0},
    {0x1151B0, "Cnd_SpaceEnter", 2},    {0x115184, "Cnd_SpaceExit", 0},      {0x115158, "Cnd_SystemEnter", 0},
    {0x11512C, "Cnd_SystemExit", 0},    {0x115884, "Cnd_Timer", 2},          {0x1158D0, "Cnd_TLEntered", 0},
    {0x1158EC, "Cnd_TLExited", 0},      {0x115908, "Cnd_True", 0},           {0x115A40, "Cnd_WatchTrigger", 1},
    {0x115A5C, "Cnd_WatchVibe", 0},
};

const char* CondName(u32 timeDateStamp, u32 sizeOfImage, u32 vtRva, int* status)
{
    if (status)
        *status = 0;
    if (timeDateStamp != 0x3ECBB142 || sizeOfImage != 0x145000 || !vtRva)
        return nullptr;
    for (const CondVt& c : kCond11)
        if (c.rva == vtRva)
        {
            if (status)
                *status = c.status;
            return c.status > 0 ? c.name : nullptr;
        }
    return nullptr;
}

// the scripted-only name, for the debug output's "?Cnd_X" hint (never returned by CondName)
static const char* CondNameScripted(u32 timeDateStamp, u32 sizeOfImage, u32 vtRva)
{
    if (timeDateStamp != 0x3ECBB142 || sizeOfImage != 0x145000)
        return nullptr;
    for (const CondVt& c : kCond11)
        if (c.rva == vtRva)
            return c.name;
    return nullptr;
}
} // namespace live

namespace
{
// Content.dll's [TriggerSave] writer (RVA 0x1AD8F in 1.1, 0x1B1FF in 1.0): mov eax,[ebx+<list head>]; mov edi,[eax];
// ...; mov eax,[edi+<value>]; mov ecx,[eax+<id>]; ...; mov eax,[edi+8]; add eax,<record>; mov eax,[eax+<cond>]; mov
// edx,[eax]; ...; call [edx+14h] (Condition::Save). Unique in v1.0, v1.1 and HD Edition 0.7.1.
const char* const kWriterSig = "8B 83 ?? ?? ?? ?? 8B 38 83 C4 0C 3B F8 74 ?? 8B FF 68 ?? ?? ?? ?? 56 FF D5 8B 47 08 8B 48 0C "
                               "51 68 ?? ?? ?? ?? 56 FF D5 8B 47 08 83 C0 10 8B 40 04 8B 10 83 C4 14 56 8B C8 FF 52 14";
// The story path's store of pub::Controller::Create's result (Content.dll RVA 0x6C2F in 1.1: call [Create]; push 0;
// push 1000h; push <name>; mov [<story id>],eax; call ...). Unique in v1.0 and v1.1.
const char* const kStoryIdSig = "FF 15 ?? ?? ?? ?? 6A 00 68 00 10 00 00 68 ?? ?? ?? ?? A3 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 4C 24 38";
// Server.dll's controller lookup by id (RVA 0x63600 in 1.1): mov edx,[<list head>]; mov ecx,[edx]; ...; cmp [ecx+18h],esi
const char* const kCtlSig = "8B 15 ?? ?? ?? ?? 8B 0A 56 33 C0 3B CA 57 8B 7C 24 0C 74 16 8B 37 39 71 18";

bool GameRead(uintptr_t addr, void* out, size_t len, void*) { return addr >= 0x10000 && safemem::Read(addr, out, len); }

bool PeInfo(HMODULE m, u32& ts, u32& soi)
{
    const uintptr_t b = reinterpret_cast<uintptr_t>(m);
    u32 lfanew = 0, sig = 0;
    return safemem::Read(b + 0x3C, &lfanew, 4) && lfanew < 0x1000 && safemem::Read(b + lfanew, &sig, 4) &&
           sig == IMAGE_NT_SIGNATURE && safemem::Read(b + lfanew + 8, &ts, 4) &&
           safemem::Read(b + lfanew + 24 + 56, &soi, 4); // FileHeader.TimeDateStamp, OptionalHeader.SizeOfImage
}

struct Resolved
{
    HMODULE content = nullptr, server = nullptr;
    u32 ts = 0, soi = 0, serverSoi = 0;
    bool done = false;
    const char* fail = nullptr; // a missing signature: cached until either DLL is another load
    live::Layout l;
};

u8 Byte(uintptr_t a)
{
    u8 v = 0;
    safemem::Read(a, &v, 1);
    return v;
}

u32 Dword(uintptr_t a)
{
    u32 v = 0;
    safemem::Read(a, &v, 4);
    return v;
}

// The layout from the game's own code, once per Content.dll / Server.dll load. nullptr = usable, else why not.
const char* Resolve(Resolved& r)
{
    HMODULE c = GetModuleHandleA("content.dll"), s = GetModuleHandleA("server.dll");
    if (!c)
        return "no Content.dll (no single-player game loaded)";
    if (!s)
        return "no Server.dll";
    u32 ts = 0, soi = 0, sts = 0, ssoi = 0;
    if (!PeInfo(c, ts, soi) || !PeInfo(s, sts, ssoi))
        return "a module header is unreadable";
    if (r.done && r.content == c && r.server == s && r.ts == ts && r.soi == soi && r.serverSoi == ssoi)
        return r.fail; // (no rescans: a missing signature stays missing for this load)
    r = Resolved{};
    r.done = true, r.content = c, r.server = s, r.ts = ts, r.soi = soi, r.serverSoi = ssoi;
    live::Layout& l = r.l;
    const uintptr_t cb = reinterpret_cast<uintptr_t>(c), sb = reinterpret_cast<uintptr_t>(s);
    l.content = cb, l.contentEnd = cb + soi;
    const uintptr_t w = hk::FindInModule("content.dll", kWriterSig, 0);
    const uintptr_t st = hk::FindInModule("content.dll", kStoryIdSig, 0);
    const uintptr_t ct = hk::FindInModule("server.dll", kCtlSig, 0);
    if (!w || !st || !ct)
        r.fail = !w ? "the [TriggerSave] writer's signature is missing in Content.dll (another build)"
                 : !st ? "the story id store's signature is missing in Content.dll (another build)"
                       : "the controller lookup's signature is missing in Server.dll (another build)";
    else
    {
        l.listHeadOff = Dword(w + 2);
        l.listValOff = Byte(w + 27);
        l.idOff = Byte(w + 30);
        l.recOff = Byte(w + 45);
        l.condOff = Byte(w + 48);
        l.storyIdAt = Dword(st + 19);
        l.ctlHeadAt = Dword(ct + 2);
        l.ctlIdOff = Byte(ct + 24);
        if (!l.listHeadOff || l.listHeadOff > 0x1000 || l.idOff > 0x40 || l.recOff > 0x40 || l.condOff > 0x40 ||
            l.storyIdAt < cb || l.storyIdAt >= cb + soi || l.ctlHeadAt < sb || l.ctlHeadAt >= sb + ssoi)
            r.fail = "the signatures' operands are implausible (another build)";
    }
    if (r.fail)
        Log("mission live: %s - the live trigger table is off for this load", r.fail);
    else
        Log("mission live: Content.dll %08X (stamp %08X) story id RVA 0x%X, active list +0x%X (value +%u), node id "
            "+0x%X state +0x%X cond +0x%X; Server.dll controller list head RVA 0x%X (id +0x%X)",
            static_cast<u32>(cb), ts, static_cast<u32>(l.storyIdAt - cb), l.listHeadOff, l.listValOff, l.idOff,
            l.recOff, l.recOff + l.condOff, static_cast<u32>(l.ctlHeadAt - sb), l.ctlIdOff);
    return r.fail;
}

Resolved g_res;
} // namespace

LiveTable LiveTriggers()
{
    static std::string lastReason;
    LiveTable t;
    const char* why = Resolve(g_res);
    std::vector<live::Raw> raw;
    if (!why)
        why = live::Walk(g_res.l, GameRead, nullptr, t.controller, raw);
    if (why)
    {
        t.reason = why;
        if (t.controller)
            t.reason += " (story id " + std::to_string(t.controller) + ")";
        if (t.reason != lastReason)
            Log("mission live: none - %s", t.reason.c_str());
        lastReason = t.reason;
        t.controller = 0;
        return t;
    }
    lastReason.clear();
    t.ok = true;
    std::vector<u32> ids;
    for (const live::Raw& r : raw)
    {
        LiveTrigger lt;
        lt.id = r.id;
        lt.state = r.state;
        if (r.condVt >= g_res.l.content && r.condVt < g_res.l.contentEnd)
        {
            lt.condRva = static_cast<u32>(r.condVt - g_res.l.content);
            lt.cond = live::CondName(g_res.ts, g_res.soi, lt.condRva);
        }
        lt.condField[0] = r.f[0], lt.condField[1] = r.f[1];
        t.triggers.push_back(lt);
        ids.push_back(r.id);
    }
    std::vector<std::string> nick, cnd;
    if (NameTriggers(ids, t.script, nick, cnd))
        for (size_t i = 0; i < t.triggers.size(); ++i)
            t.triggers[i].nickname = nick[i], t.triggers[i].iniCond = cnd[i];
    return t;
}

const char* TriggerStateName(int state)
{
    return state == 0 ? "armed" : state == 1 ? "inactive" : state == 2 ? "complete" : "?";
}

bool Command(const std::string& cmd, const std::string& rest, std::string* reply)
{
    if (cmd != "mission")
        return false;
    std::istringstream in(rest);
    std::string sub, opt;
    in >> sub >> opt;
    if (sub != "live")
    {
        *reply = "usage: mission live [raw]";
        return true;
    }
    const bool raw = opt == "raw";
    const LiveTable t = LiveTriggers();
    std::ostringstream out;
    if (!t.ok)
    {
        out << "mission live: none - " << t.reason;
        *reply = out.str();
        return true;
    }
    const size_t slash = t.script.find_last_of("\\/");
    out << "mission live: controller=" << t.controller
        << " script=" << (t.script.empty() ? "?" : slash == std::string::npos ? t.script : t.script.substr(slash + 1))
        << " n=" << t.triggers.size();
    if (raw)
    {
        char line[200];
        const live::Layout& l = g_res.l;
        sprintf_s(line, " | content=%08X stamp=%08X size=%X storyIdRva=0x%X list=+0x%X node id+0x%X state+0x%X cond+0x%X",
                  static_cast<u32>(l.content), g_res.ts, g_res.soi, static_cast<u32>(l.storyIdAt - l.content),
                  l.listHeadOff, l.idOff, l.recOff, l.recOff + l.condOff);
        out << line;
    }
    for (const LiveTrigger& lt : t.triggers)
    {
        const char* scripted = live::CondNameScripted(g_res.ts, g_res.soi, lt.condRva);
        // the type to read the fields by: the verified vtable name, else the ini's key
        std::string type = lt.cond ? lt.cond : lt.iniCond;
        std::transform(type.begin(), type.end(), type.begin(), [](char ch) { return static_cast<char>(tolower(ch)); });
        char line[320];
        int n = sprintf_s(line, "\n  %u %s %s vt=+0x%X cond=%s%s ini=%s", lt.id,
                          lt.nickname.empty() ? "?" : lt.nickname.c_str(), TriggerStateName(lt.state), lt.condRva,
                          lt.cond ? "" : (scripted ? "?" : ""), lt.cond ? lt.cond : (scripted ? scripted : "-"),
                          lt.iniCond.empty() ? "-" : lt.iniCond.c_str());
        if (n < 0)
            n = 0;
        float f0 = 0, f1 = 0;
        memcpy(&f0, &lt.condField[0], 4);
        memcpy(&f1, &lt.condField[1], 4);
        if (type == "cnd_timer")
            n += sprintf_s(line + n, sizeof line - n, " elapsed=%.2f/%.2f", f0, f1);
        else if (type == "cnd_destroyed")
            n += sprintf_s(line + n, sizeof line - n, " count=%u", lt.condField[0]);
        else if (type == "cnd_watchtrigger")
            n += sprintf_s(line + n, sizeof line - n, " watch=%u", lt.condField[0]);
        else if (raw)
            n += sprintf_s(line + n, sizeof line - n, " f=%08X,%08X", lt.condField[0], lt.condField[1]);
        // the vtable's name (also a scripted one) against the ini's: a mismatch means the map is wrong for this build
        const char* vtName = lt.cond ? lt.cond : scripted;
        if (vtName && !lt.iniCond.empty() && _stricmp(vtName, lt.iniCond.c_str()) != 0)
            sprintf_s(line + n, sizeof line - n, " MISMATCH");
        out << line;
    }
    *reply = out.str();
    return true;
}
} // namespace mission
