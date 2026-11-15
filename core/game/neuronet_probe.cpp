// FLvibed - the NeuroNet text screens' proving experiment (audit 70b work order W5, results audit_2026-11-12\81_neuronet_probe.md):
// the debug verb `nnprobe`, test tooling only, reached through the debug channel (which exists only with the test switch,
// core\common.h TestSwitch) and refusing anyway without it.
//
// It rebuilds, from the client's own stores and with NO window open, what the game's mission log (NeuroNetStoryStar), job
// board (NN_MissionVendor) and news (NN_News) windows show, in the order and with the formatting their own code uses (read
// in the disassembly: audit 81c), and with a window open reads the window's own records and texts beside it, so the two can
// be compared line by line. It makes the knowledge reveal those windows make (RevealText, audit 81b) on the same texts, with
// the guards that keep it from crashing, and predicts its effect on the visit map from the game's own knowledge map; and it
// sends a job's acceptance by the request the board's ACCEPT ends in (IServer MissionResponse, audit 81a), recording the
// server's answer (the acceptance packet's own handler, MissionAcceptRecv) and in which frame it came.
//
//   nnprobe state                          base, record, mirrors, log, knowledge map, visit map size, open windows
//   nnprobe visit snap <name> | diff <name> | get <id>   the visit map: keep a copy, list what changed since, one id
//   nnprobe log [mission|log]              the log as the window would compose that tab (default: the tab it opens on)
//   nnprobe jobs | news                    the offers / stories from the base record, in the window's order, formatted
//   nnprobe chain jobs|news <row>          each shown FmtStr's ids and values (kind = value), as the reveal reads them
//   nnprobe window log|jobs|news           the open window's own records, selection and texts
//   nnprobe compare log|jobs|news          the composed text against the open window's: EQUAL or the lines that differ
//   nnprobe predict jobs|news <row>        what the window's reveal of that row would change in the visit map
//   nnprobe reveal jobs|news <row>         that reveal, direct (RevealText on the row's FmtStrs, in the window's order),
//                                          then the visit map's change against the prediction
//   nnprobe may accept <row>               the board's refusal for ACCEPT on that row (0 = the board would send it)
//   nnprobe accept <row>                   the direct acceptance (MissionResponse with the row's two ids), refused first
//                                          where the board would not send it
//   nnprobe answers [clear]                the acceptance answers recorded: frame, inside the request or not, the verdict
//   nnprobe open log                       the game's own command for the mission log (as its NeuroNet button)
// Single player only (multiplayer stays on the game's own buttons until a test-server run proves the route).
#include "common.h"
#include "flexe.h"
#include "hook.h"
#include "log.h"
#include "safemem.h"
#include "game/baseinfo.h"
#include "game/coreframe.h"
#include "game/iserver.h"
#include "game/nnwin.h"
#include "game/room.h"
#include "game/space.h"
#include "text.h"
#include "ui/winplates.h"

#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace debug
{
namespace
{
// ---- guarded reads
u32 R32(uintptr_t a)
{
    u32 v = 0;
    return a && safemem::Read(a, &v, 4) ? v : 0;
}
u16 R16(uintptr_t a)
{
    u16 v = 0;
    return a && safemem::Read(a, &v, 2) ? v : 0;
}
u8 R8(uintptr_t a)
{
    u8 v = 0;
    return a && safemem::Read(a, &v, 1) ? v : 0;
}

// ---- Common.dll: the functions the windows and the reveal import (by export name: the same functions)
struct Common
{
    const void*(__cdecl* getBase)(u32) = nullptr;        // Universe::get_base: 0 for an unknown id
    const void*(__cdecl* getSystem)(u32) = nullptr;      // Universe::get_system
    const void*(__cdecl* getZone)(u32) = nullptr;        // Universe::get_zone (the one-argument overload)
    u32(__cdecl* repName)(u32) = nullptr;                // Reputation::get_name: a faction's ids_name
    bool(__cdecl* singlePlayer)() = nullptr;
    void(__thiscall* rdlWriterMake)(void*) = nullptr;    // BinaryRDLWriter::BinaryRDLWriter
    bool(__thiscall* rdlWrite)(void*, const void*, char*, u32, u32&) = nullptr; // BinaryRDLWriter::write_buffer
    void(__thiscall* mbMake)(void*) = nullptr;           // MarshalBuffer::MarshalBuffer
    void(__thiscall* mbUse)(void*, const void*, int) = nullptr; // MarshalBuffer::use_read_buffer
    void(__thiscall* mvaMake)(void*) = nullptr;          // MissionVendorAcceptance::MissionVendorAcceptance
    void(__thiscall* mvaRead)(void*, void*) = nullptr;   // MissionVendorAcceptance::unmarshal
    void(__thiscall* mvaFree)(void*) = nullptr;          // MissionVendorAcceptance::~MissionVendorAcceptance
    bool ok = false;
};
const Common& C()
{
    static Common c = []
    {
        Common r;
        HMODULE m = GetModuleHandleA("common.dll");
        if (!m)
            return r;
        auto get = [m](auto& p, const char* n) { p = reinterpret_cast<std::remove_reference_t<decltype(p)>>(GetProcAddress(m, n)); };
        get(r.getBase, "?get_base@Universe@@YAPAUIBase@1@I@Z");
        get(r.getSystem, "?get_system@Universe@@YAPBUISystem@1@I@Z");
        get(r.getZone, "?get_zone@Universe@@YAPBUIZone@1@I@Z");
        get(r.repName, "?get_name@Reputation@@YAII@Z");
        get(r.singlePlayer, "?SinglePlayer@@YA_NXZ");
        get(r.rdlWriterMake, "??0BinaryRDLWriter@@QAE@XZ");
        get(r.rdlWrite, "?write_buffer@BinaryRDLWriter@@UAE_NABVRenderDisplayList@@PADIAAI@Z");
        get(r.mbMake, "??0MarshalBuffer@@QAE@XZ");
        get(r.mbUse, "?use_read_buffer@MarshalBuffer@@QAEXPBEH@Z");
        get(r.mvaMake, "??0MissionVendorAcceptance@@QAE@XZ");
        get(r.mvaRead, "?unmarshal@MissionVendorAcceptance@@QAEXPAVMarshalBuffer@@@Z");
        get(r.mvaFree, "??1MissionVendorAcceptance@@QAE@XZ");
        r.ok = r.getBase && r.getSystem && r.getZone && r.repName && r.singlePlayer && r.rdlWriterMake && r.rdlWrite && r.mbMake && r.mbUse &&
               r.mvaMake && r.mvaRead && r.mvaFree;
        return r;
    }();
    return c;
}
// An object's virtual that takes no argument and returns an id (IBase vt+8, ISystem vt+0xC, IZone vt+0x84: their ids_name).
u32 VirtualId(const void* obj, u32 slot)
{
    const u32 fn = obj ? R32(R32(reinterpret_cast<uintptr_t>(obj)) + slot) : 0;
    return fn ? reinterpret_cast<u32(__thiscall*)(const void*)>(fn)(obj) : 0;
}

// ---- text: the game's two formatters (SEH-guarded: a bad FmtStr must not take the game down)
int FormatHashes(const void* fs, wchar_t* out, int max) // exe::FmtStrFormat: values are hashes (offers, news, objectives)
{
    __try
    {
        return hk::Call<int(__fastcall*)(const void*, void*, wchar_t*, int)>(exe::FmtStrFormat, fs, nullptr, out, max);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
        return -1;
    }
}
int FormatLog(const void* fs, wchar_t* out, int max, bool allowM) // exe::LogFormat: values are string ids (log entries)
{
    __try
    {
        return hk::Call<int(__cdecl*)(const void*, wchar_t*, int, bool)>(exe::LogFormat, fs, out, max, allowM);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        out[0] = 0;
        return -1;
    }
}
std::wstring Fmt(const void* fs)
{
    static wchar_t buf[0x1000];
    buf[0] = 0;
    if (!fs || !exe::FmtStrFormat.ok() || FormatHashes(fs, buf, 0x1000) < 0)
        return L"<format failed>";
    buf[0xFFF] = 0;
    return buf;
}
std::wstring FmtLog(const void* fs, bool allowM)
{
    static wchar_t buf[0x1000];
    buf[0] = 0;
    if (!fs || !exe::LogFormat.ok() || FormatLog(fs, buf, 0x1000, allowM) < 0)
        return L"<format failed>";
    buf[0xFFF] = 0;
    return buf;
}
// The reward cell as the board composes it (0x48C8A0): 1857 "TBD" for 0, else 1842 "$ %d0" with the number.
std::wstring Reward(int reward)
{
    if (reward == 0)
        return text::Ids(1857);
    std::wstring t = text::Ids(1842), n = std::to_wstring(reward);
    // %d0 formats with thousands commas (0x4779A0, audit 81c 1)
    for (int at = static_cast<int>(n.size()) - 3; at > 0; at -= 3)
        n.insert(static_cast<size_t>(at), L",");
    const size_t p = t.find(L"%d0");
    if (p != std::wstring::npos)
        t.replace(p, 3, n);
    return t;
}
std::string N(const std::wstring& w) { return text::Utf8(w); }
std::wstring Collapse(const std::wstring& s) // whitespace runs (spaces, line ends, a paragraph) as one space; for comparing
{
    std::wstring o;
    bool sp = false;
    for (const wchar_t c : s)
    {
        const bool w = c == L' ' || c == L'\n' || c == L'\r' || c == L'\t';
        if (w)
            sp = !o.empty();
        else
        {
            if (sp)
                o.push_back(L' ');
            sp = false;
            o.push_back(c);
        }
    }
    return o;
}

// ---- a rich text control's display list, written out by Common.dll's BinaryRDLWriter (the INFO page's route, FREELANCER_RE
// "The INFO page's composers"): records {u32 type, u32 length, payload}: 2 TEXT (UTF-16), 5 PARA, 6 STYLE (u16)
struct Rich
{
    std::wstring text; // TEXT records, a PARA as "\n"
    std::vector<u16> styles;
    bool ok = false;
};
u32 WriteRdl(const void* rdl, char* out, u32 cap)
{
    __try
    {
        void* writer[4] = {};
        C().rdlWriterMake(writer);
        u32 written = 0;
        return C().rdlWrite(writer, rdl, out, cap, written) ? written : 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return 0;
    }
}
Rich ReadRich(u32 ctl)
{
    Rich r;
    if (!ctl || !exe::VtText.is(R32(ctl)))
        return r;
    static std::vector<char> buf(256 * 1024);
    const u32 n = WriteRdl(reinterpret_cast<const void*>(ctl + 0x488), buf.data(), static_cast<u32>(buf.size()));
    r.ok = true;
    for (u32 at = 0; at + 8 <= n;)
    {
        u32 type = 0, len = 0;
        memcpy(&type, buf.data() + at, 4);
        memcpy(&len, buf.data() + at + 4, 4);
        if (at + 8 + len > n)
            break;
        const char* p = buf.data() + at + 8;
        if (type == 2)
            for (u32 i = 0; i + 1 < len; i += 2)
            {
                wchar_t c = 0;
                memcpy(&c, p + i, 2);
                if (!c)
                    break;
                r.text.push_back(c);
            }
        else if (type == 5)
            r.text.push_back(L'\n');
        else if (type == 6 && len >= 2)
        {
            u16 s = 0;
            memcpy(&s, p, 2);
            r.styles.push_back(s);
        }
        at += 8 + len;
    }
    return r;
}
std::wstring Plain(const char* name)
{
    std::wstring t;
    winplates::TextOf(uiclick::Find(name), t);
    return t;
}

// ---- the base record (RoomInfoOwner's map, walked here: BaseRecord on an id missing from it reads address 4)
u32 DockedBase() { return exe::RoomInfoOwner.ok() ? R32(exe::RoomInfoOwner.get() + 0x34) : 0; }
uintptr_t Record()
{
    const u32 base = DockedBase();
    if (!base || base != Mem<u32>(exe::PlayerBase) || !exe::RoomInfoOwner.ok())
        return 0;
    const uintptr_t head = R32(exe::RoomInfoOwner.get() + 0x24);
    uintptr_t n = R32(head + 4);
    for (int i = 0; n && n != head && i < 64; ++i)
    {
        const u32 key = R32(n + 0xC);
        if (key == base)
            return R32(R32(n + 0x10) + 4);
        n = base < key ? R32(n) : R32(n + 8);
    }
    return 0;
}
// A mirror (the offers at record +0, the news at +0x18): {+4 head}, nodes {+0 next, +4 prev, +8 the properties}, ends at 0.
std::vector<uintptr_t> Mirror(uintptr_t list)
{
    std::vector<uintptr_t> out;
    for (uintptr_t n = list ? R32(list + 4) : 0; n && out.size() < 256; n = R32(n))
        out.push_back(R32(n + 8));
    return out;
}
std::vector<uintptr_t> Offers() { return Mirror(Record()); }
// The news in the window's order (0x4A4FD0): four passes over the mirror, each taking what the ones before left: byte +9 set,
// dword +0xC == 1, byte +8 set, the rest.
std::vector<uintptr_t> News()
{
    const uintptr_t rec = Record();
    const std::vector<uintptr_t> m = Mirror(rec ? rec + 0x18 : 0);
    std::vector<uintptr_t> out;
    std::vector<bool> taken(m.size());
    for (int pass = 0; pass < 4; ++pass)
        for (size_t i = 0; i < m.size(); ++i)
        {
            const uintptr_t p = m[i];
            const bool take = pass == 0 ? R8(p + 9) != 0 : pass == 1 ? R32(p + 0xC) == 1 : pass == 2 ? R8(p + 8) != 0 : true;
            if (!taken[i] && take)
                taken[i] = true, out.push_back(p);
        }
    return out;
}
// The FmtStrs a row shows and the window reveals, in the window's order: an offer's destination, title, full text; a story's
// headline, body.
std::vector<uintptr_t> Shown(bool jobs, uintptr_t props)
{
    if (!props)
        return {};
    if (jobs)
        return {props + 0x14, props + 0x2C, props + 0x44};
    return {props + 0x28, props + 0x40};
}

// ---- the open windows (by their registry names, the class checked by vtable)
u32 Window(const char* name, hk::Sym& vt)
{
    const u32 w = uiclick::Find(name);
    return w && vt.is(R32(w)) ? w : 0;
}
u32 BoardWindow() { return Window("NN_MissionVendor", exe::VtMissionVendor); }
u32 NewsWindow() { return Window("NN_News", exe::VtNews); }
u32 LogWindow()
{
    // NeuroNetStoryStar's registry name is unread: the history text's control, then its owner by class
    const u32 text = uiclick::Find("StoryStarHistoryText");
    if (!text)
        return 0;
    for (u32 w = R32(text + 4), i = 0; w && i < 8; w = R32(w + 4), ++i) // (control +4: its parent, as uiclick::Click reads it)
        if (exe::VtStoryStar.is(R32(w)))
            return w;
    return 0;
}

// ---- FmtStr chains: node {+0 next, +4 value, +8 kind (wchar), +0xA index}
struct Val
{
    wchar_t kind;
    u32 value;
};
std::vector<Val> Chain(uintptr_t fs)
{
    std::vector<Val> out;
    for (uintptr_t n = R32(fs); n && out.size() < 64; n = R32(n))
        out.push_back({static_cast<wchar_t>(R16(n + 8)), R32(n + 4)});
    return out;
}
// Would RevealText survive this FmtStr? Its `B` and `S` values go to Universe::get_base / get_system and the result is used
// with no test; a `!` value is a nested FmtStr read at once (audit 81b 1).
bool RevealSafe(uintptr_t fs, int depth, std::string& why)
{
    if (!fs || depth > 8)
        return why = "a null or too deep FmtStr", false;
    for (const Val& v : Chain(fs))
    {
        if (v.kind == L'B' && !C().getBase(v.value))
            return why = "unknown base " + std::to_string(v.value), false;
        if (v.kind == L'S' && !C().getSystem(v.value))
            return why = "unknown system " + std::to_string(v.value), false;
        if (v.kind == L'!' && !RevealSafe(v.value, depth + 1, why))
            return false;
    }
    return true;
}
// The ids RevealText hands the knowledge map for this FmtStr (0x459950): its own ids, then per value: `s` the value, `B` / `S`
// / `Z` the object's ids_name, `F` the faction's, `!` the nested FmtStr's; the other kinds nothing.
void RevealKeys(uintptr_t fs, int depth, std::vector<u32>& keys)
{
    if (!fs || depth > 8)
        return;
    keys.push_back(R32(fs + 4));
    for (const Val& v : Chain(fs))
        switch (v.kind)
        {
        case L's': keys.push_back(v.value); break;
        case L'B': keys.push_back(VirtualId(C().getBase(v.value), 8)); break;
        case L'S': keys.push_back(VirtualId(C().getSystem(v.value), 0xC)); break;
        case L'F': keys.push_back(C().repName(v.value)); break;
        case L'Z': keys.push_back(VirtualId(C().getZone(v.value), 0x84)); break;
        case L'!': RevealKeys(v.value, depth + 1, keys); break;
        default: break;
        }
}
void PutChain(std::ostringstream& o, uintptr_t fs, int depth)
{
    o << std::string(2 * depth + 2, ' ') << "ids=" << R32(fs + 4);
    for (const Val& v : Chain(fs))
        o << " " << static_cast<char>(v.kind) << "=" << v.value;
    o << "\n";
    if (depth < 4)
        for (const Val& v : Chain(fs))
            if (v.kind == L'!' && v.value)
                PutChain(o, v.value, depth + 1);
}

// ---- the visit map (the client's st6::map<u32, u8> at exe::VisitMap: +4 head, +8 nil, +0x10 size; node +0 left, +8 right,
// +0xC id, +0x10 flags)
using Visits = std::map<u32, u8>;
bool ReadVisits(Visits& out)
{
    out.clear();
    if (!exe::VisitMap.ok())
        return false;
    const uintptr_t vm = exe::VisitMap.get(), head = R32(vm + 4), nil = R32(vm + 8);
    const u32 size = R32(vm + 0x10);
    std::vector<uintptr_t> stack;
    if (const uintptr_t root = R32(head + 4); root && root != nil && root != head)
        stack.push_back(root);
    for (int guard = 0; !stack.empty() && guard < 20000; ++guard)
    {
        const uintptr_t n = stack.back();
        stack.pop_back();
        out[R32(n + 0xC)] = R8(n + 0x10);
        for (const uintptr_t c : {static_cast<uintptr_t>(R32(n)), static_cast<uintptr_t>(R32(n + 8))})
            if (c && c != nil && c != head)
                stack.push_back(c);
    }
    return out.size() == size;
}
std::string Diff(const Visits& a, const Visits& b)
{
    std::ostringstream o;
    int n = 0;
    for (const auto& [id, f] : b)
    {
        const auto it = a.find(id);
        if (it == a.end())
            o << " +" << id << "=" << int(f), ++n;
        else if (it->second != f)
            o << " " << id << ":" << int(it->second) << "->" << int(f), ++n;
    }
    for (const auto& [id, f] : a)
        if (!b.count(id))
            o << " -" << id, ++n;
    return n ? o.str().substr(1) : "none";
}
std::map<std::string, Visits> g_snaps;
// What the reveal of these FmtStrs would change (0x459210 per key): every knowledge-map entry {id, flags} of the key, the
// visit byte becoming old | flags where it lacks a flag (or is absent), entries applied in order (one may follow another).
Visits Predict(const std::vector<uintptr_t>& texts, const Visits& now)
{
    Visits changed;
    const uintptr_t km = exe::KnowledgeMap.ok() ? R32(exe::KnowledgeMap.get()) : 0;
    if (!km)
        return changed;
    const uintptr_t head = R32(km + 4), nil = R32(km + 8);
    auto find = [&](u32 key) -> uintptr_t
    {
        uintptr_t n = R32(head + 4);
        for (int i = 0; n && n != nil && n != head && i < 64; ++i)
        {
            const u32 k = R32(n + 0xC);
            if (k == key)
                return n;
            n = key < k ? R32(n) : R32(n + 8);
        }
        return 0;
    };
    for (const uintptr_t fs : texts)
    {
        std::vector<u32> keys;
        RevealKeys(fs, 0, keys);
        for (const u32 key : keys)
            if (const uintptr_t node = key ? find(key) : 0)
                for (uintptr_t p = node + 0x10, i = 0; p && i < 32; p = R32(p + 8), ++i)
                {
                    const u32 id = R32(p);
                    const u8 f = R8(p + 4);
                    const auto c = changed.find(id);
                    const auto w = now.find(id);
                    const bool has = c != changed.end() || w != now.end();
                    const u8 old = c != changed.end() ? c->second : w != now.end() ? w->second : 0;
                    if (!has || (old & f) != f)
                        changed[id] = static_cast<u8>(old | f);
                }
    }
    return changed;
}
std::string PutPredict(const Visits& now, const Visits& changed)
{
    std::ostringstream o;
    int n = 0;
    for (const auto& [id, f] : changed)
    {
        const auto w = now.find(id);
        if (w == now.end())
            o << " +" << id << "=" << int(f);
        else
            o << " " << id << ":" << int(w->second) << "->" << int(f);
        ++n;
    }
    return n ? o.str().substr(1) : "none";
}

// ---- the log (NeuroNetStoryStar's view 0x4BC2A0, audit 81c 1)
struct LogEntry
{
    std::wstring text;  // formatted with allowM (the view's entry text)
    std::wstring plain; // formatted without (the view's highlight test)
    u32 ids = 0;
};
std::vector<LogEntry> LogEntries()
{
    std::vector<LogEntry> out;
    if (!exe::MissionLog.ok())
        return out;
    const uintptr_t sentinel = R32(exe::MissionLog.get());
    for (uintptr_t n = R32(sentinel); n && n != sentinel && out.size() < 512; n = R32(n))
    {
        const void* fs = reinterpret_cast<const void*>(n + 8);
        out.push_back({FmtLog(fs, true), FmtLog(fs, false), R32(n + 0xC)});
    }
    return out;
}
u32 LogCount() { return exe::MissionLog.ok() ? R32(exe::MissionLog.get() + 4) : 0; }
u32 LogHead() { return exe::MissionLog.ok() ? R32(R32(exe::MissionLog.get())) : 0; }
bool DeathMode() { return exe::NNManager.ok() && R32(exe::NNManager.get()) && R8(R32(exe::NNManager.get()) + 0x52F) != 0; }
bool RouteListEmpty()
{
    const uintptr_t head = exe::RouteList.ok() ? R32(exe::RouteList.get()) : 0;
    return !head || R32(head) == head;
}
int OpeningTab() { return RouteListEmpty() && !DeathMode() && C().singlePlayer() ? 0 : 1; } // 0 LOG, 1 MISSION (0x4BBF15)
struct Composed
{
    std::wstring text;
    std::vector<u16> styles; // per shown entry: 0x3D0A (the current objective, a death) or 0x9CA4
};
Composed ComposeLog(int tab)
{
    Composed c;
    const std::vector<LogEntry> all = LogEntries();
    if (all.empty())
    {
        c.text = text::Ids(1039);
        return c;
    }
    // the current objectives, for the MISSION tab's highlight: each as text without %M (the view resolves and formats with
    // allowM 0; here the client's copy is formatted with its hashes: audit 81_ notes the difference if any shows)
    std::vector<std::wstring> objectives;
    if (exe::ClientObjective.ok())
        for (int i = 0; i < 32; ++i)
        {
            const void* o = hk::Call<const void*(__cdecl*)(int)>(exe::ClientObjective, i);
            if (!o)
                break;
            objectives.push_back(Fmt(o));
        }
    bool highlighted = false, first = true;
    for (const LogEntry& e : all)
    {
        const bool star = !e.text.empty() && e.text[0] == L'*';
        if ((tab == 0) != star)
            continue;
        std::wstring t = star ? e.text.substr(1) : e.text;
        u16 style = 0x9CA4;
        if (tab == 1 && !highlighted)
            for (const std::wstring& ob : objectives)
                if (ob == e.plain)
                {
                    style = 0x3D0A, highlighted = true;
                    break;
                }
        if (e.ids == 1827)
            style = 0x3D0A;
        else if (e.text.find(L':') == std::wstring::npos)
            t = text::Ids(tab == 0 ? 1596 : DeathMode() && first ? 987 : 1574) + t;
        if (!first)
            c.text += L"\n\n";
        c.text += t;
        c.styles.push_back(style);
        first = false;
    }
    return c;
}

// ---- the acceptance answer: the exe's receive table entry that holds MissionAcceptRecv (found by its value, not by an
// address), chained: the game's handler runs as before, after the answer is recorded. Test games only, release build: a hot
// reload of the logic would leave the table pointing into the old DLL, so the verb installs it only on `accept` / `answers`.
struct Answer
{
    u32 frame = 0;
    bool inRequest = false; // arrived while the probe's own MissionResponse call was still on the stack
    int accepted = -1;      // MissionVendorAcceptance +8
    std::wstring text;      // its FmtStr +0xC formatted (the board shows it as the refusal)
};
std::vector<Answer> g_answers;
bool g_inRequest = false, g_pending = false;
using RecvFn = void(__stdcall*)(u32, const void*, int);
RecvFn g_origRecv = nullptr;
uintptr_t g_recvSlot = 0;
wchar_t g_answerText[0x400];
int DecodeAcceptance(const void* buf, int len) // the packet read as 0x4379B0 reads it, into a copy of the game's own type
{
    alignas(8) unsigned char mb[0x40] = {}, mva[0x60] = {};
    g_answerText[0] = 0;
    __try
    {
        C().mbMake(mb);
        C().mbUse(mb, buf, len);
        C().mvaMake(mva);
        C().mvaRead(mva, mb);
        const int accepted = mva[8];
        FormatHashes(mva + 0xC, g_answerText, 0x400);
        C().mvaFree(mva);
        return accepted;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return -2;
    }
}
void __stdcall AcceptRecv(u32 a, const void* buf, int len)
{
    Answer an;
    an.frame = coreframe::g_frame;
    an.inRequest = g_inRequest;
    an.accepted = C().ok ? DecodeAcceptance(buf, len) : -3;
    g_answerText[0x3FF] = 0;
    an.text = g_answerText;
    if (g_answers.size() < 64)
        g_answers.push_back(an);
    g_pending = false;
    Log("nnprobe: acceptance answer frame %u accepted %d%s", an.frame, an.accepted, an.inRequest ? " (inside the request)" : "");
    g_origRecv(a, buf, len);
}
bool InstallRecv(std::string& why)
{
    if (g_recvSlot)
        return true;
    if (!exe::MissionAcceptRecv.ok() || !C().ok)
        return why = "MissionAcceptRecv or a Common.dll export unresolved", false;
    const u32 fn = static_cast<u32>(exe::MissionAcceptRecv.get());
    const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    uintptr_t hit = 0;
    int hits = 0;
    for (int s = 0; s < nt->FileHeader.NumberOfSections; ++s)
        if (!(sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE))
            for (uintptr_t a = base + sec[s].VirtualAddress, end = a + sec[s].Misc.VirtualSize; a + 4 <= end; a += 4)
                if (R32(a) == fn)
                    hit = a, ++hits;
    if (hits != 1)
        return why = "MissionAcceptRecv is in " + std::to_string(hits) + " table entries, not one", false;
    DWORD old = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(hit), 4, PAGE_READWRITE, &old))
        return why = "the table is not writable", false;
    g_origRecv = reinterpret_cast<RecvFn>(Mem<u32>(hit));
    Mem<u32>(hit) = reinterpret_cast<u32>(&AcceptRecv);
    VirtualProtect(reinterpret_cast<void*>(hit), 4, old, &old);
    g_recvSlot = hit;
    Log("nnprobe: the acceptance answer is recorded (receive table entry 0x%08X)", static_cast<u32>(hit));
    return true;
}
std::string PutAnswers()
{
    std::ostringstream o;
    o << "answers " << g_answers.size() << " pending=" << (g_pending ? 1 : 0) << " frame=" << coreframe::g_frame << "\n";
    for (const Answer& a : g_answers)
        o << "  frame=" << a.frame << " in_request=" << (a.inRequest ? 1 : 0) << " accepted=" << a.accepted << " text=\"" << N(Collapse(a.text)) << "\"\n";
    return o.str();
}

// ---- the board's ACCEPT (0x48C460 case 0x26): it sends only with a row selected on a board that has offers (the window builds
// no button for an empty one); rank, reputation, "already on a mission" (IDS 1840) are the server's answer (audit 81a 2-3)
int MayAccept(int row, std::string& why)
{
    if (!C().singlePlayer())
        return why = "multiplayer: the game's own button", 1;
    const uintptr_t rec = Record();
    if (!baseinfo::Docked() || !rec)
        return why = "not docked, or no record for the docked base", 2;
    const std::vector<uintptr_t> offers = Offers();
    if (!R8(rec + 0x14) || offers.empty())
        return why = "the board has no offers (the window shows no ACCEPT)", 3;
    if (row < 0 || row >= static_cast<int>(offers.size()))
        return why = "no such row (the window's ACCEPT needs a selection)", 4;
    if (g_pending)
        return why = "an acceptance sent and not answered (the probe's rule; the window would send a second)", 5;
    if (const u32 w = BoardWindow(); w && R32(w + 0x33C))
        return why = "the open board has a callback the direct route would skip", 6;
    if (!exe::ServerIface.ok() || !R32(exe::ServerIface.get()) || !exe::OwnClientId.ok())
        return why = "no server interface", 7;
    return why = "the board would send it", 0;
}

// ---- output
void PutRows(std::ostringstream& o, bool jobs, const std::vector<uintptr_t>& rows)
{
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const uintptr_t p = rows[i];
        if (jobs)
            o << "row " << i << " key=" << R32(p) << " id8=" << R32(p + 8) << " idC=" << R32(p + 0xC) << " type=" << R32(p + 0x10)
              << " reward=" << static_cast<int>(R32(p + 0x5C)) << " | " << N(Fmt(reinterpret_cast<const void*>(p + 0x14))) << " | "
              << N(Fmt(reinterpret_cast<const void*>(p + 0x2C))) << " | " << N(Reward(static_cast<int>(R32(p + 0x5C)))) << "\n  text: "
              << N(Collapse(Fmt(reinterpret_cast<const void*>(p + 0x44)))) << "\n";
        else
            o << "row " << i << " key=" << R32(p) << " b8=" << int(R8(p + 8)) << " b9=" << int(R8(p + 9)) << " type=" << R32(p + 0xC) << " | "
              << N(Fmt(reinterpret_cast<const void*>(p + 0x28))) << "\n  text: " << N(Collapse(Fmt(reinterpret_cast<const void*>(p + 0x40)))) << "\n";
    }
}
struct Cmp
{
    std::ostringstream o;
    int differ = 0, checked = 0;
    void Eq(const std::string& what, const std::wstring& window, const std::wstring& direct)
    {
        ++checked;
        const bool eq = Collapse(window) == Collapse(direct);
        if (!eq)
            ++differ;
        o << (eq ? "  EQUAL  " : "  DIFFER ") << what << ": window \"" << N(Collapse(window)) << "\"";
        if (!eq)
            o << " | direct \"" << N(Collapse(direct)) << "\"";
        o << "\n";
    }
    void EqN(const std::string& what, long long window, long long direct) { Eq(what, std::to_wstring(window), std::to_wstring(direct)); }
    std::string Done(const std::string& head)
    {
        return head + (differ ? " DIFFER " : " EQUAL ") + std::to_string(checked - differ) + "/" + std::to_string(checked) + "\n" + o.str();
    }
};
std::string CompareJobs(u32 w)
{
    Cmp c;
    const std::vector<uintptr_t> offers = Offers();
    const int n = static_cast<int>(R32(w + 0x398)), first = static_cast<int>(R32(w + 0x370)), sel = static_cast<int>(R32(w + 0x378));
    const uintptr_t recs = R32(w + 0x394);
    c.EqN("count", n, static_cast<long long>(offers.size()));
    for (int i = 0; i < n && i < static_cast<int>(offers.size()); ++i)
        c.EqN("row " + std::to_string(i) + " key", R32(recs + 0x60 * i), R32(offers[i]));
    for (int k = 0; k < 5 && first + k < static_cast<int>(offers.size()); ++k)
    {
        const uintptr_t p = offers[first + k];
        char nm[48];
        sprintf_s(nm, "MissionVendorSummaryDest%d", k);
        c.Eq("shown row " + std::to_string(k) + " dest", Plain(nm), Fmt(reinterpret_cast<const void*>(p + 0x14)));
        sprintf_s(nm, "MissionVendorSummaryTitle%d", k);
        c.Eq("shown row " + std::to_string(k) + " title", Plain(nm), Fmt(reinterpret_cast<const void*>(p + 0x2C)));
        sprintf_s(nm, "MissionVendorSummaryReward%d", k);
        c.Eq("shown row " + std::to_string(k) + " reward", Plain(nm), Reward(static_cast<int>(R32(p + 0x5C))));
    }
    if (sel >= 0 && sel < static_cast<int>(offers.size()))
    {
        const uintptr_t p = offers[sel];
        c.Eq("detail dest", Plain("MissionVendorMissionDest"), Fmt(reinterpret_cast<const void*>(p + 0x14)));
        c.Eq("detail title", Plain("MissionVendorMissionTitle"), Fmt(reinterpret_cast<const void*>(p + 0x2C)));
        c.Eq("detail reward", Plain("MissionVendorMissionReward"), Reward(static_cast<int>(R32(p + 0x5C))));
        c.Eq("detail text", ReadRich(uiclick::Find("MissionVendorMissionFullText")).text, Fmt(reinterpret_cast<const void*>(p + 0x44)));
    }
    else if (sel == -1) // after a refusal (0x48D4A0): "Mission Rejected", the three heads cleared, the server's reason as the text
    {
        std::wstring reason = L"<no refusal recorded>";
        for (auto it = g_answers.rbegin(); it != g_answers.rend(); ++it)
            if (it->accepted == 0)
            {
                reason = it->text;
                break;
            }
        c.Eq("rejected caption", Plain("MissionVendorMissionReject"), text::Ids(1839));
        c.Eq("rejected dest", Plain("MissionVendorMissionDest"), L"");
        c.Eq("rejected title", Plain("MissionVendorMissionTitle"), L"");
        c.Eq("rejected reward", Plain("MissionVendorMissionReward"), L"");
        c.Eq("rejected text", ReadRich(uiclick::Find("MissionVendorMissionFullText")).text, reason);
    }
    return c.Done("compare jobs: selected=" + std::to_string(sel) + " first=" + std::to_string(first));
}
std::string CompareNews(u32 w)
{
    Cmp c;
    const std::vector<uintptr_t> news = News();
    const int n = static_cast<int>(R32(w + 0x588)), first = static_cast<int>(R32(w + 0x578)), sel = static_cast<int>(R32(w + 0x580));
    const uintptr_t recs = R32(w + 0x584);
    c.EqN("count", n, static_cast<long long>(news.size()));
    for (int i = 0; i < n && i < static_cast<int>(news.size()); ++i)
        c.EqN("row " + std::to_string(i) + " key", R32(recs + 0x60 * i), R32(news[i]));
    for (int k = 0; k < 5 && first + k < static_cast<int>(news.size()); ++k)
    {
        char nm[48];
        sprintf_s(nm, "NewsHeadlineHeadline%d", k);
        c.Eq("shown row " + std::to_string(k) + " headline", Plain(nm), Fmt(reinterpret_cast<const void*>(news[first + k] + 0x28)));
    }
    if (sel >= 0 && sel < static_cast<int>(news.size()))
    {
        c.Eq("story headline", Plain("NewsStoryHeadline"), Fmt(reinterpret_cast<const void*>(news[sel] + 0x28)));
        c.Eq("story text", ReadRich(uiclick::Find("NewsStoryText")).text, Fmt(reinterpret_cast<const void*>(news[sel] + 0x40)));
    }
    return c.Done("compare news: selected=" + std::to_string(sel) + " first=" + std::to_string(first));
}
std::string PutStyles(const std::vector<u16>& s)
{
    std::ostringstream o;
    for (const u16 v : s)
        o << " 0x" << std::hex << v;
    return o.str();
}
std::string CompareLog(u32 w)
{
    Cmp c;
    const int tab = R8(w + 0x37C);
    c.EqN("seen count", R32(w + 0x338), LogCount());
    c.Eq("title", Plain("StoryStarTitle"), text::Ids(C().singlePlayer() ? 1552 : 1583));
    const Rich win = ReadRich(uiclick::Find("StoryStarHistoryText"));
    const Composed mine = ComposeLog(tab);
    c.Eq(tab == 0 ? "LOG text" : "MISSION text", win.text, mine.text);
    c.o << "  styles: window" << PutStyles(win.styles) << " | composed per entry" << PutStyles(mine.styles) << "\n";
    return c.Done("compare log: tab=" + std::string(tab == 0 ? "LOG" : "MISSION") + " opening rule says " + (OpeningTab() == 0 ? "LOG" : "MISSION"));
}
std::string PutState()
{
    std::ostringstream o;
    const uintptr_t rec = Record();
    Visits v;
    const bool whole = ReadVisits(v);
    o << "frame=" << coreframe::g_frame << " sp=" << (C().singlePlayer() ? 1 : 0) << " docked=" << (baseinfo::Docked() ? 1 : 0) << " base=" << DockedBase()
      << " record=0x" << std::hex << rec << std::dec << " offers=" << Offers().size() << " offers_complete=" << int(rec ? R8(rec + 0x14) : 0)
      << " news=" << News().size() << " news_complete=" << int(rec ? R8(rec + 0x2C) : 0) << " log=" << LogCount() << " log_head=0x" << std::hex
      << LogHead() << " knowledge_map=0x" << R32(exe::KnowledgeMap.get()) << std::dec << " visits=" << v.size() << (whole ? "" : " (partial read)")
      << " death=" << (DeathMode() ? 1 : 0) << " routelist_empty=" << (RouteListEmpty() ? 1 : 0) << " opening_tab=" << (OpeningTab() == 0 ? "LOG" : "MISSION")
      << "\n";
    const u32 b = BoardWindow(), nw = NewsWindow(), lw = LogWindow();
    o << "windows board=" << (b ? 1 : 0) << " news=" << (nw ? 1 : 0) << " log=" << (lw ? 1 : 0);
    if (b)
        o << " | board count=" << R32(b + 0x398) << " sel=" << static_cast<int>(R32(b + 0x378)) << " first=" << R32(b + 0x370) << " callback=0x" << std::hex
          << R32(b + 0x33C) << " cookie=0x" << R32(b + 0x340) << std::dec << " accepted_flag=" << int(R8(b + 0x344));
    if (nw)
        o << " | news count=" << R32(nw + 0x588) << " sel=" << static_cast<int>(R32(nw + 0x580)) << " first=" << R32(nw + 0x578);
    if (lw)
        o << " | log tab=" << (R8(lw + 0x37C) == 0 ? "LOG" : "MISSION") << " seen=" << R32(lw + 0x338);
    o << "\n";
    return o.str();
}
std::string Usage()
{
    return "nnprobe state | visit snap|diff <name> | visit get <id> | log [mission|log] | jobs | news | chain jobs|news <row> | window log|jobs|news | "
           "compare log|jobs|news | predict jobs|news <row> | reveal jobs|news <row> | may accept <row> | accept <row> | answers [clear] | open log";
}
std::string VisitVerb(std::istream& in)
{
    std::string side, name;
    in >> side >> name;
    Visits v;
    const bool whole = ReadVisits(v);
    if (side == "snap")
    {
        g_snaps[name] = v;
        return "nnprobe visit snap " + name + ": " + std::to_string(v.size()) + " ids" + (whole ? "" : " (partial read)");
    }
    if (side == "diff")
    {
        const auto it = g_snaps.find(name);
        if (it == g_snaps.end())
            return "nnprobe visit diff: no snapshot " + name;
        return "nnprobe visit diff " + name + ": " + std::to_string(it->second.size()) + " -> " + std::to_string(v.size()) + " ids: " + Diff(it->second, v);
    }
    if (side == "get")
    {
        const u32 id = static_cast<u32>(strtoul(name.c_str(), nullptr, 0));
        const auto it = v.find(id);
        return "nnprobe visit " + std::to_string(id) + ": " + (it == v.end() ? "absent" : std::to_string(it->second));
    }
    return Usage();
}
std::string WindowVerb(const std::string& what, const std::string& side)
{
    if (side == "log")
    {
        const u32 w = LogWindow();
        if (!w)
            return "nnprobe " + what + " log: no log window";
        if (what == "compare")
            return CompareLog(w);
        const Rich r = ReadRich(uiclick::Find("StoryStarHistoryText"));
        return "window log tab=" + std::string(R8(w + 0x37C) == 0 ? "LOG" : "MISSION") + " seen=" + std::to_string(R32(w + 0x338)) + " title=\"" +
               N(Plain("StoryStarTitle")) + "\" styles" + PutStyles(r.styles) + "\n" + N(r.text) + "\n";
    }
    const bool j = side == "jobs";
    if (!j && side != "news")
        return Usage();
    const u32 w = j ? BoardWindow() : NewsWindow();
    if (!w)
        return "nnprobe " + what + " " + side + ": no window";
    if (what == "compare")
        return j ? CompareJobs(w) : CompareNews(w);
    const int n = static_cast<int>(R32(w + (j ? 0x398 : 0x588)));
    const uintptr_t recs = R32(w + (j ? 0x394 : 0x584));
    std::vector<uintptr_t> rows;
    for (int i = 0; i < n && i < 256; ++i)
        rows.push_back(recs + 0x60 * i);
    std::ostringstream o;
    PutRows(o, j, rows);
    return "window " + side + " count=" + std::to_string(n) + " sel=" + std::to_string(static_cast<int>(R32(w + (j ? 0x378 : 0x580)))) + "\n" + o.str();
}
// chain / predict / reveal of one row's shown texts
std::string TextVerb(const std::string& what, const std::string& side, int row)
{
    const bool j = side == "jobs";
    if (!j && side != "news")
        return Usage();
    const std::vector<uintptr_t> rows = j ? Offers() : News();
    if (row < 0 || row >= static_cast<int>(rows.size()))
        return "nnprobe " + what + ": no row " + std::to_string(row) + " (" + std::to_string(rows.size()) + " rows)";
    const std::vector<uintptr_t> texts = Shown(j, rows[row]);
    std::ostringstream o;
    if (what == "chain")
    {
        for (const uintptr_t fs : texts)
        {
            PutChain(o, fs, 0);
            std::vector<u32> keys;
            RevealKeys(fs, 0, keys);
            o << "    keys";
            for (const u32 k : keys)
                o << " " << k;
            o << "\n";
        }
        return "chain " + side + " " + std::to_string(row) + "\n" + o.str();
    }
    std::string why;
    for (const uintptr_t fs : texts)
        if (!RevealSafe(fs, 0, why))
            return "nnprobe " + what + ": refused, the reveal would crash: " + why;
    Visits before;
    if (!ReadVisits(before))
        return "nnprobe " + what + ": the visit map did not read whole";
    const Visits predicted = Predict(texts, before);
    if (what == "predict")
        return "predict " + side + " " + std::to_string(row) + ": " + PutPredict(before, predicted);
    if (!C().singlePlayer() || !baseinfo::Docked() || !Record() || !R32(exe::KnowledgeMap.get()))
        return "nnprobe reveal: refused (single player, docked, the base's record and the knowledge map are needed)";
    const u32 f0 = coreframe::g_frame;
    for (const uintptr_t fs : texts) // RevealText's own order in the window: destination, title, text / headline, body
        hk::Call<void(__cdecl*)(const void*)>(exe::RevealText, reinterpret_cast<const void*>(fs));
    Visits after;
    const bool whole = ReadVisits(after);
    Visits actual;
    for (const auto& [id, f] : after)
        if (const auto it = before.find(id); it == before.end() || it->second != f)
            actual[id] = f;
    const bool eq = whole && actual == predicted;
    return "reveal " + side + " " + std::to_string(row) + " frame=" + std::to_string(f0) + ": " + (eq ? "EQUAL" : "DIFFER") + " to the prediction\n  predicted: " +
           PutPredict(before, predicted) + "\n  actual:    " + Diff(before, after) + "\n";
}
std::string AcceptVerb(int row)
{
    std::string why;
    const int r = MayAccept(row, why);
    if (r)
        return "nnprobe accept: refused " + std::to_string(r) + " " + why;
    if (!InstallRecv(why))
        return "nnprobe accept: refused, the answer cannot be recorded: " + why;
    const uintptr_t p = Offers()[row];
    const u32 a = R32(p + 8), b = R32(p + 0xC), client = Mem<u32>(exe::OwnClientId);
    const uintptr_t server = R32(exe::ServerIface.get());
    const u32 fn = R32(R32(server) + iserver::kMissionResponse);
    const size_t had = g_answers.size();
    const u32 f0 = coreframe::g_frame;
    g_pending = true;
    g_inRequest = true;
    reinterpret_cast<void(__thiscall*)(uintptr_t, u32, u32, bool, u32)>(fn)(server, a, b, true, client);
    g_inRequest = false;
    std::ostringstream o;
    o << "accept row " << row << " key=" << R32(p) << " MissionResponse(" << a << ", " << b << ", true, " << client << ") frame=" << f0
      << " answers during the call: " << (g_answers.size() - had) << "\n" << PutAnswers();
    return o.str();
}
} // namespace
} // namespace debug

// The verb (debugverbs.cpp's dispatch declares it at its call, so at global scope). Test tooling: refuses without the test
// switch and in multiplayer.
std::string NNProbe(std::istream& in)
{
    using namespace debug;
    if (!TestSwitch())
        return "nnprobe: test games only";
    if (space::Multiplayer())
        return "nnprobe: single player only";
    if (!C().ok || !exe::FmtStrFormat.ok() || !exe::LogFormat.ok() || !exe::MissionLog.ok() || !exe::VisitMap.ok() || !exe::KnowledgeMap.ok() ||
        !exe::RevealText.ok() || !exe::RoomInfoOwner.ok())
        return "nnprobe: a symbol or a Common.dll export is unresolved";
    std::string what, side;
    in >> what;
    if (what == "state" || what.empty())
        return PutState();
    if (what == "open")
    {
        in >> side;
        if (side != "log")
            return Usage();
        nnwin::Press(0x67); // the mission log's command (NNMissionLogButtonRoot: FREELANCER_RE "FLvibed's native buttons")
        return "nnprobe open: command 0x67 sent";
    }
    if (what == "visit")
        return VisitVerb(in);
    if (what == "log")
    {
        in >> side;
        const int tab = side == "log" ? 0 : side == "mission" ? 1 : OpeningTab();
        const Composed c = ComposeLog(tab);
        return "log tab=" + std::string(tab == 0 ? "LOG" : "MISSION") + " entries=" + std::to_string(LogCount()) + " title=\"" +
               N(text::Ids(C().singlePlayer() ? 1552 : 1583)) + "\" styles" + PutStyles(c.styles) + "\n" + N(c.text) + "\n";
    }
    if (what == "jobs" || what == "news")
    {
        const std::vector<uintptr_t> rows = what == "jobs" ? Offers() : News();
        std::ostringstream o;
        PutRows(o, what == "jobs", rows);
        return what + " " + std::to_string(rows.size()) + "\n" + o.str();
    }
    if (what == "window" || what == "compare")
    {
        in >> side;
        return WindowVerb(what, side);
    }
    if (what == "chain" || what == "predict" || what == "reveal")
    {
        int row = -1;
        in >> side >> row;
        return TextVerb(what, side, row);
    }
    if (what == "may")
    {
        int row = -1;
        in >> side >> row;
        std::string why;
        const int r = MayAccept(row, why);
        return "may accept " + std::to_string(row) + ": " + std::to_string(r) + " " + why;
    }
    if (what == "accept")
    {
        int row = -1;
        in >> row;
        return AcceptVerb(row);
    }
    if (what == "answers")
    {
        in >> side;
        std::string why;
        if (!InstallRecv(why))
            return "nnprobe answers: " + why;
        if (side == "clear")
            g_answers.clear(), g_pending = false;
        return PutAnswers();
    }
    return Usage();
}
